#include "lom_native_audio_categories.h"
#include <psyz/spu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../support/lom_check.h"

static const int stereo_channels = 2;
static const int cd_left_sample = 3276;
static const int cd_right_sample_magnitude = 1638;
static const int right_volume_offset = 2;
static const int pitch_offset = 4;
static const int voice_pitch = 0x1000;
static const int sample_start_offset = 6;
static const int sample_address_shift = 3;
static const int adsr_low_offset = 8;
static const int voice_adsr_attack = 0x000F;
static const int adsr_high_offset = 10;
static const int voice_adsr_sustain = 0x0000;
static const int repeat_offset = 14;
static const int spu_register_bytes = 0x200;
static const int read_only_status_register = 0x1AE;
static const int fixture_voice_gain = 0x1800;
static const int master_left_register = 0x180;
static const int master_register_gain = 0x3FFF;
static const int master_right_register = 0x182;
static const int spu_control_register = 0x1AA;
static const int reverb_control_bits = 0xC080;
static const int spu_enabled_bits = 0xC000;
static const int reverb_work_register = 0x1A2;
static const int reverb_left_register = 0x184;
static const int reverb_unity_coefficient = 0x4000;
static const int reverb_right_register = 0x186;
static const int reverb_register_start = 0x1C0;
static const int maximum_signed_sample = 0x7FFF;
static const int reverb_wall_coefficient = 0x6000;
static const int reverb_comb_coefficient = 0x5000;
static const int maximum_command_byte = 0xFF;
static const int probe_voice_left_register = 0xC0;
static const int reverb_send_register = 0x198;
static const int key_on_register = 0x188;
static const int full_volume = 100;
static const int half_volume = 50;
static const int quarter_volume = 25;
static const int invalid_volume = 101;
static const int key_off_register = 0x18C;
static const int spu_cd_enabled_bits = 0xC001;
static const int cd_left_register = 0x1B0;
static const int cd_right_register = 0x1B2;
static const char success_message[] =
    "audio-categories: the original mix at 100%, each group on its own, release and reuse, reverb and the real SPU state all hold";
static const unsigned reverb_work_start_units = 0xC000;
static const unsigned short reverb_register_setup[][2] = {
    {reverb_work_register, reverb_work_start_units},
    {reverb_left_register, reverb_unity_coefficient},
    {reverb_right_register, reverb_unity_coefficient},
    {reverb_register_start, 1},
    {0x1C2, 1},
    {0x1C4, maximum_signed_sample},
    {0x1C6, reverb_wall_coefficient},
    {0x1CE, reverb_comb_coefficient},
    {0x1D0, reverb_unity_coefficient},
    {0x1D2, reverb_unity_coefficient},
    {0x1D4, 0x20},
    {0x1D6, 0x40},
    {0x1D8, 0x20},
    {0x1DA, 0x40},
    {0x1E0, 0x1F},
    {0x1E2, 0x3F},
    {0x1E4, 0x100},
    {0x1E6, 0x120},
    {0x1F0, 0x11F},
    {0x1F2, maximum_command_byte},
    {0x1F4, 0x60},
    {0x1F6, 0x80},
    {0x1F8, 0xA0},
    {0x1FA, probe_voice_left_register},
    {0x1FC, reverb_unity_coefficient},
    {0x1FE, reverb_unity_coefficient},
};
static const unsigned reverb_register_column = 0;
static const unsigned reverb_value_column = 1;
static const unsigned register_stride = 16;
static const unsigned envelope_register_offset = 12;
static const int music_voice_mask = 1;
static const int sfx_voice_mask = 2;
static const int both_voice_mask = 3;
static const int half_gain_divisor = 2;
static const int quarter_gain_divisor = 4;
static const int right_voice_register_offset = 2;
static const int byte_bits = 8;

enum
{
    FRAMES = 2048,
    SAMPLE = 0x2000,
    MUSIC = 0,
    SFX = 12
};

static const unsigned char adpcm[16] = {0x04, 0x07, 0x77, 0x77, 0x77, 0x77, 0x99, 0x99, 0x99, 0x99, 0x55, 0x55, 0x55, 0xBB, 0xBB, 0xBB};
static short original[FRAMES * 2], actual[FRAMES * 2], expected[FRAMES * 2], warm[FRAMES * 2];
static unsigned char original_ram[PSYZ_SPU_RAM_SIZE];
static int cd_enabled;

/**
 * @brief Supply deterministic stereo CD samples when the fixture enables CD audio.
 * @param out Destination stereo sample buffer.
 * @param frames Number of stereo frames requested.
 * @return Frames supplied, or zero while CD input is disabled.
 */
size_t __wrap_Psyz_CdPullSamples(short* out, size_t frames)
{
    if (!cd_enabled)
    {
        return 0;
    }

    for (size_t i = 0; i < frames; ++i)
    {
        out[stereo_channels * i] = cd_left_sample;
        out[stereo_channels * i + 1] = -cd_right_sample_magnitude;
    }

    return frames;
}

/**
 * @brief Measure absolute sample amplitude across the fixture buffer.
 * @param samples Sample buffer to inspect or fill.
 * @return Sum of absolute sample magnitudes.
 */
static long long energy(const short* samples)
{
    long long sum = 0;

    for (int i = 0; i < FRAMES * stereo_channels; ++i)
    {
        sum += abs(samples[i]);
    }

    return sum;
}

/**
 * @brief Configure a physical SPU voice with the looping fixture sample.
 * @param v Physical SPU voice index.
 * @param left Fixture left-channel voice gain.
 * @param right Fixture right-channel voice gain.
 */
static void voice(int v, int left, int right)
{
    unsigned base = v * register_stride;
    Psyz_SpuWrite(base, left);
    Psyz_SpuWrite(base + right_volume_offset, right);
    Psyz_SpuWrite(base + pitch_offset, voice_pitch);
    Psyz_SpuWrite(base + sample_start_offset, SAMPLE >> sample_address_shift);
    Psyz_SpuWrite(base + adsr_low_offset, voice_adsr_attack);
    Psyz_SpuWrite(base + adsr_high_offset, voice_adsr_sustain);
    Psyz_SpuWrite(base + repeat_offset, SAMPLE >> sample_address_shift);
}

/**
 * @brief Reset SPU registers, fixture voices and optional reverb history.
 * @param voices Bitmask selecting fixture music and SFX voices.
 * @param reverb Whether to enable fixture reverb.
 */
static void reset(int voices, int reverb)
{
    Psyz_SpuReset(0);

    for (unsigned r = 0; r < spu_register_bytes; r += sfx_voice_mask)
    {
        if (r != read_only_status_register)
        {
            Psyz_SpuWrite(r, 0);
        }
    }

    Psyz_SpuMemWrite(SAMPLE, adpcm, sizeof(adpcm));
    voice(MUSIC, fixture_voice_gain, 0);
    voice(SFX, 0, fixture_voice_gain);
    Psyz_SpuWrite(master_left_register, master_register_gain);
    Psyz_SpuWrite(master_right_register, master_register_gain);
    Psyz_SpuWrite(spu_control_register, reverb ? reverb_control_bits : spu_enabled_bits);

    if (reverb)
    {
        for (size_t index = 0; index < sizeof(reverb_register_setup) / sizeof(reverb_register_setup[0]); ++index)
        {
            Psyz_SpuWrite(reverb_register_setup[index][reverb_register_column], reverb_register_setup[index][reverb_value_column]);
        }

        Psyz_SpuWrite(reverb_send_register, (1u << MUSIC) | (1u << SFX));
    }

    lom_native_spu_set_sfx_mask(1u << SFX);
    unsigned mask = ((voices & 1) ? 1u << MUSIC : 0) | ((voices & sfx_voice_mask) ? 1u << SFX : 0);
    Psyz_SpuWrite(key_on_register, mask);
}

/**
 * @brief Apply category controls and render warmed-up fixture samples.
 * @param voices Bitmask selecting fixture music and SFX voices.
 * @param reverb Whether to enable fixture reverb.
 * @param music Music output percentage.
 * @param sfx SFX output percentage.
 * @param samples Sample buffer to inspect or fill.
 */
static void render(int voices, int reverb, int music, int sfx, short* samples)
{
    lom_native_spu_set_category_volumes(music, sfx);
    reset(voices, reverb);
    Psyz_SpuPullSamples(warm, FRAMES);
    Psyz_SpuPullSamples(samples, FRAMES);
}

/**
 * @brief Verify independent dry gains, invalid values and key-off/key-on ownership reuse.
 */
static void dry_contract(void)
{
    render(both_voice_mask, 0, full_volume, full_volume, original);
    CHECK(energy(original) > 0);
    render(both_voice_mask, 0, 0, full_volume, actual);

    for (int i = 0; i < FRAMES; ++i)
    {
        CHECK(actual[half_gain_divisor * i] == 0 && actual[half_gain_divisor * i + 1] == original[half_gain_divisor * i + 1]);
    }

    render(both_voice_mask, 0, full_volume, 0, actual);

    for (int i = 0; i < FRAMES; ++i)
    {
        CHECK(actual[half_gain_divisor * i] == original[half_gain_divisor * i] && actual[half_gain_divisor * i + 1] == 0);
    }

    render(both_voice_mask, 0, half_volume, quarter_volume, actual);

    for (int i = 0; i < FRAMES; ++i)
    {
        CHECK(abs(actual[half_gain_divisor * i] - original[half_gain_divisor * i] / half_gain_divisor) <= 1);
        CHECK(abs(actual[half_gain_divisor * i + 1] - original[half_gain_divisor * i + 1] / quarter_gain_divisor) <= 1);
    }

    render(both_voice_mask, 0, 0, 0, actual);
    CHECK(energy(actual) == 0);
    CHECK(Psyz_SpuRead(MUSIC * register_stride + envelope_register_offset) > 0 && Psyz_SpuRead(SFX * register_stride + envelope_register_offset) > 0);
    lom_native_spu_set_category_volumes(-1, full_volume);
    lom_native_spu_set_category_volumes(full_volume, invalid_volume);
    Psyz_SpuPullSamples(actual, FRAMES);
    CHECK(energy(actual) == 0);
    lom_native_spu_set_sfx_mask(0);
    lom_native_spu_set_category_volumes(full_volume, 0);
    Psyz_SpuWrite(key_off_register, 1u << SFX);
    Psyz_SpuPullSamples(actual, FRAMES);

    for (int i = 0; i < FRAMES; ++i)
    {
        CHECK(actual[half_gain_divisor * i + 1] == 0);
    }

    Psyz_SpuWrite(key_on_register, 1u << SFX);
    Psyz_SpuPullSamples(actual, FRAMES);
    CHECK(energy(actual) > 0);
    int right = 0;

    for (int i = 0; i < FRAMES; ++i)
    {
        right |= actual[half_gain_divisor * i + 1];
    }

    CHECK(right);
    lom_native_spu_set_sfx_mask(1u << MUSIC);
    Psyz_SpuWrite(key_on_register, 1u << MUSIC);
    Psyz_SpuPullSamples(actual, FRAMES);

    for (int i = 0; i < FRAMES; ++i)
    {
        CHECK(actual[half_gain_divisor * i] == 0);
    }
}

/**
 * @brief Verify isolated reverb tails while original SPU RAM and envelopes remain intact.
 */
static void reverb_contract(void)
{
    render(sfx_voice_mask, 1, full_volume, full_volume, expected);
    CHECK(energy(expected) > 0);
    render(both_voice_mask, 1, full_volume, full_volume, actual);
    lom_native_spu_set_category_volumes(0, full_volume);
    Psyz_SpuPullSamples(actual, FRAMES);
    render(sfx_voice_mask, 1, full_volume, full_volume, expected);
    Psyz_SpuPullSamples(expected, FRAMES);
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    render(both_voice_mask, 1, full_volume, full_volume, actual);
    lom_native_spu_set_category_volumes(full_volume, 0);
    Psyz_SpuPullSamples(actual, FRAMES);
    render(music_voice_mask, 1, full_volume, full_volume, expected);
    Psyz_SpuPullSamples(expected, FRAMES);
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    Psyz_SpuWrite(MUSIC * register_stride, 0);
    Psyz_SpuWrite(MUSIC * register_stride + right_voice_register_offset, 0);
    Psyz_SpuPullSamples(actual, FRAMES);
    CHECK(energy(actual) > 0);
    lom_native_spu_set_category_volumes(0, full_volume);
    Psyz_SpuPullSamples(actual, FRAMES);
    CHECK(energy(actual) == 0);
    render(both_voice_mask, 1, full_volume, full_volume, original);
    memcpy(original_ram, Psyz_SpuGetRam(), sizeof(original_ram));
    unsigned env_music = Psyz_SpuRead(MUSIC * register_stride + envelope_register_offset),
             env_sfx = Psyz_SpuRead(SFX * register_stride + envelope_register_offset);
    render(both_voice_mask, 1, 0, full_volume, actual);
    CHECK(!memcmp(original_ram, Psyz_SpuGetRam(), sizeof(original_ram)));
    CHECK(env_music == Psyz_SpuRead(MUSIC * register_stride + envelope_register_offset) &&
          env_sfx == Psyz_SpuRead(SFX * register_stride + envelope_register_offset));
}

/**
 * @brief Verify CD follows music gain while original capture buffers stay live.
 */
static void cd_contract(void)
{
    cd_enabled = 1;
    lom_native_spu_set_category_volumes(full_volume, full_volume);
    reset(0, 0);
    Psyz_SpuWrite(spu_control_register, spu_cd_enabled_bits);
    Psyz_SpuWrite(cd_left_register, maximum_signed_sample);
    Psyz_SpuWrite(cd_right_register, maximum_signed_sample);
    Psyz_SpuPullSamples(original, FRAMES);
    CHECK(energy(original) > 0);
    lom_native_spu_set_category_volumes(half_volume, 0);
    Psyz_SpuPullSamples(actual, FRAMES);

    for (int i = 0; i < FRAMES * stereo_channels; ++i)
    {
        CHECK(abs(actual[i] - original[i] / stereo_channels) <= 1);
    }

    lom_native_spu_set_category_volumes(0, full_volume);
    Psyz_SpuPullSamples(actual, FRAMES);
    CHECK(energy(actual) == 0);
    unsigned char capture[stereo_channels];
    Psyz_SpuMemRead(0, capture, stereo_channels);
    CHECK((capture[0] | (capture[1] << byte_bits)) == cd_left_sample);
    cd_enabled = 0;
}

/**
 * @brief Verify category gains, voice ownership and private reverb against the real SPU.
 * @return Zero on success; failed assertions terminate the process.
 */
int main(void)
{
    Psyz_SpuInit();
    reset(both_voice_mask, 1);
    Psyz_SpuPullSamples(warm, FRAMES);
    Psyz_SpuPullSamples(original, FRAMES);
    memcpy(original_ram, Psyz_SpuGetRam(), sizeof(original_ram));
    render(both_voice_mask, 1, full_volume, full_volume, actual);
    CHECK(!memcmp(original, actual, sizeof(original)));
    CHECK(!memcmp(original_ram, Psyz_SpuGetRam(), sizeof(original_ram)));
    dry_contract();
    reverb_contract();
    cd_contract();
    puts(success_message);
    return 0;
}
