#include <psyz/spu.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOICE0_BASE 0x00
#define VOICE1_BASE 0x10
#define SAMPLE0_ADDR 0x2000
#define SAMPLE1_ADDR 0x2100
#define SPU_MAIN_VOL_LEFT 0x180
#define SPU_MAIN_VOL_RIGHT 0x182
#define SPU_KEY_ON_LOW 0x188
#define SPU_PMON_LOW 0x190
#define SPU_NOISE_LOW 0x194
#define SPU_CONTROL 0x1AA
#define FRAME_COUNT 1024
#define WARMUP_FRAMES 96

static const unsigned char s_modulator_adpcm[16] = {
    0x00, 0x07, 0x77, 0x77, 0x77, 0x77, 0x99, 0x99, 0x99, 0x99, 0x77, 0x77, 0x77, 0x77, 0x99, 0x99,
};

static const unsigned char s_carrier_adpcm[16] = {
    0x00, 0x07, 0x77, 0x77, 0x99, 0x99, 0x77, 0x77, 0x99, 0x99, 0x77, 0x77, 0x99, 0x99, 0x77, 0x77,
};

static const unsigned char s_silent_adpcm[16] = {
    0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/**
 * @brief Set up an SPU voice to play a sample, looping.
 * @param voice_base The voice's first register.
 * @param sample_addr The sample's address in SPU RAM.
 * @param pitch Its pitch, 0x1000 for its own rate.
 * @param volume Its volume on both sides.
 */
static void setup_voice(int voice_base, unsigned sample_addr, unsigned pitch, unsigned volume)
{
    Psyz_SpuWrite(voice_base + 0x00, (unsigned short)volume);
    Psyz_SpuWrite(voice_base + 0x02, (unsigned short)volume);
    Psyz_SpuWrite(voice_base + 0x04, (unsigned short)pitch);
    Psyz_SpuWrite(voice_base + 0x06, (unsigned short)(sample_addr >> 3));
    Psyz_SpuWrite(voice_base + 0x08, 0x000F);
    Psyz_SpuWrite(voice_base + 0x0A, 0x1FC0);
    Psyz_SpuWrite(voice_base + 0x0E, (unsigned short)(sample_addr >> 3));
}

/**
 * @brief Reset the SPU, with the main volume up and the SPU on.
 */
static void reset_spu(void)
{
    Psyz_SpuReset(0);
    Psyz_SpuWrite(SPU_MAIN_VOL_LEFT, 0x3FFF);
    Psyz_SpuWrite(SPU_MAIN_VOL_RIGHT, 0x3FFF);
    Psyz_SpuWrite(SPU_CONTROL, 0xC000);
}

/**
 * @brief Add up how loud a render is, after its warm-up.
 * @param samples The render.
 * @return The sum of its samples' sizes.
 */
static long long buffer_energy(const short* samples)
{
    long long energy = 0;
    int i;

    for (i = WARMUP_FRAMES * 2; i < FRAME_COUNT * 2; i++)
    {
        energy += llabs((long long)samples[i]);
    }

    return energy;
}

/**
 * @brief Add up how much two renders differ, after their warm-up.
 * @param a One render.
 * @param b The other.
 * @return The sum of the differences.
 */
static long long buffer_difference(const short* a, const short* b)
{
    long long difference = 0;
    int i;

    for (i = WARMUP_FRAMES * 2; i < FRAME_COUNT * 2; i++)
    {
        difference += llabs((long long)a[i] - b[i]);
    }

    return difference;
}

/**
 * @brief Check a voice set to noise makes sound from a silent sample.
 * @return 0, or 1 when it's silent.
 */
static int test_noise(void)
{
    short output[FRAME_COUNT * 2];
    long long energy;

    reset_spu();
    Psyz_SpuMemWrite(SAMPLE0_ADDR, s_silent_adpcm, sizeof(s_silent_adpcm));
    setup_voice(VOICE0_BASE, SAMPLE0_ADDR, 0x1000, 0x3FFF);
    Psyz_SpuWrite(SPU_NOISE_LOW, 1u << 0);
    Psyz_SpuWrite(SPU_CONTROL, 0xC000 | (8u << 10) | (2u << 8));
    Psyz_SpuWrite(SPU_KEY_ON_LOW, 1u << 0);
    memset(output, 0, sizeof(output));
    Psyz_SpuPullSamples(output, FRAME_COUNT);

    energy = buffer_energy(output);

    if (energy == 0)
    {
        fprintf(stderr, "noise-enabled voice produced silence\n");
        return 1;
    }

    printf("  noise energy:      %lld\n", energy);
    return 0;
}

/**
 * @brief Render a carrier voice, with or without the voice before it modulating
 *        its pitch.
 * @param output Set to the render.
 * @param pitch_modulation Nonzero to modulate it.
 */
static void render_carrier(short* output, int pitch_modulation)
{
    reset_spu();
    Psyz_SpuMemWrite(SAMPLE0_ADDR, s_modulator_adpcm, sizeof(s_modulator_adpcm));
    Psyz_SpuMemWrite(SAMPLE1_ADDR, s_carrier_adpcm, sizeof(s_carrier_adpcm));

    setup_voice(VOICE0_BASE, SAMPLE0_ADDR, 0x0800, 0x0000);
    setup_voice(VOICE1_BASE, SAMPLE1_ADDR, 0x1000, 0x3FFF);
    Psyz_SpuWrite(SPU_NOISE_LOW, 1u << 0);
    Psyz_SpuWrite(SPU_CONTROL, 0xC000 | (8u << 10) | (2u << 8));

    if (pitch_modulation)
    {
        Psyz_SpuWrite(SPU_PMON_LOW, 1u << 1);
    }

    Psyz_SpuWrite(SPU_KEY_ON_LOW, (1u << 0) | (1u << 1));
    memset(output, 0, FRAME_COUNT * 2 * sizeof(short));
    Psyz_SpuPullSamples(output, FRAME_COUNT);
}

/**
 * @brief Check pitch modulation changes the carrier's sound without silencing
 *        it.
 * @return 0, or 1 when it doesn't.
 */
static int test_pitch_modulation(void)
{
    short normal[FRAME_COUNT * 2];
    short modulated[FRAME_COUNT * 2];
    long long normal_energy;
    long long modulated_energy;
    long long difference;

    render_carrier(normal, 0);
    render_carrier(modulated, 1);

    normal_energy = buffer_energy(normal);
    modulated_energy = buffer_energy(modulated);
    difference = buffer_difference(normal, modulated);

    if (normal_energy == 0 || modulated_energy == 0)
    {
        fprintf(stderr, "pitch-modulation carrier unexpectedly silent\n");
        return 1;
    }

    if (difference == 0)
    {
        fprintf(stderr, "PMON did not change the carrier waveform\n");
        return 1;
    }

    printf("  PMON difference:   %lld\n", difference);
    return 0;
}

int main(void)
{
    Psyz_SpuInit();

    if (test_noise() != 0 || test_pitch_modulation() != 0)
    {
        return 1;
    }

    printf("LoM SPU noise/pitch-modulation smoke passed.\n");
    return 0;
}
