#include "main/audio/akao.h"
#include "main/audio/internal/akao_driver.h"
#include "lom_native_akao.h"
#include "lom_native_audio_categories.h"
#include <psyz/spu.h>
#include <stdint.h>
#include <stdio.h>

extern unsigned char lom_native_spu_registers[];

long lom_native_akao_disable_event(long event);

long lom_native_akao_enable_event(long event);

enum
{
    FRAMES = 256
};

static const int sample_ram_offset = 0x2000;

static const unsigned reverb_register_start = 0x1C0;

static const unsigned spu_register_bytes = 0x200;

static const int reverb_left_register = 0x184;

static const int reverb_right_register = 0x186;

static const int master_left_register = 0x180;

static const int master_right_register = 0x182;

static const int master_register_gain = 0x3FFF;

static const int spu_control_register = 0x1AA;

static const int spu_enabled_bits = 0xC000;

static const int key_on_register = 0x188;

static const int test_voice = 12;

static const int voice_left_register = 0xC0;

static const int voice_right_register = 0xC2;

static const int voice_pitch_register = 0xC4;

static const int voice_sample_register = 0xC6;

static const int voice_adsr_low_register = 0xC8;

static const int voice_adsr_high_register = 0xCA;

static const int voice_envelope_register = 0xCC;

static const int voice_repeat_register = 0xCE;

static const int voice_gain = 0x1800;

static const int voice_pitch = 0x1000;

static const int sample_address_units = 0x400;

static const int adsr_attack = 15;

static const int full_volume = 100;

static const unsigned char sample[] = {0, 7, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};

enum
{
    KEYED_AS_PLAYING_EFFECT,
    KEYED_AS_PAUSED_EFFECT,
    KEYED_AS_SAMPLED_PROGRAM,
    KEYED_AS_MUSIC,
    CASE_COUNT
};

static const char probe_error[] = "AKAO effect voices didn't reach the mixer as sound effects.\n";

static const char probe_success[] = "  Music and Sound effects: playing and paused effects and the sampled program count as sound effects, the rest as music.";

int lom_native_akao_category_probe(void)
{
    volatile uint16_t* registers = (volatile uint16_t*)lom_native_spu_registers;
    SfxControl saved = g_akao_sfx_control;
    s32 saved_program = g_akao_xa_tracker.voice_mask;
    short out[FRAMES * 2];
    int failed = 0;

    Psyz_SpuMemWrite(sample_ram_offset, sample, sizeof(sample));

    for (unsigned r = reverb_register_start; r < spu_register_bytes; r += 2)
    {
        Psyz_SpuWrite(r, 0);
    }

    Psyz_SpuWrite(reverb_left_register, 0);
    Psyz_SpuWrite(reverb_right_register, 0);
    Psyz_SpuWrite(master_left_register, master_register_gain);
    Psyz_SpuWrite(master_right_register, master_register_gain);
    Psyz_SpuWrite(spu_control_register, spu_enabled_bits);
    Psyz_SpuWrite(voice_left_register, voice_gain);
    Psyz_SpuWrite(voice_right_register, voice_gain);
    Psyz_SpuWrite(voice_pitch_register, voice_pitch);
    Psyz_SpuWrite(voice_sample_register, sample_address_units);
    Psyz_SpuWrite(voice_adsr_low_register, adsr_attack);
    Psyz_SpuWrite(voice_adsr_high_register, 0);
    Psyz_SpuWrite(voice_repeat_register, sample_address_units);
    lom_native_spu_set_category_volumes(0, full_volume);

    for (int kind = 0; kind < CASE_COUNT; ++kind)
    {
        lom_native_akao_disable_event(0);
        g_akao_sfx_control.active_mask = kind == KEYED_AS_PLAYING_EFFECT ? 1u << test_voice : 0;
        g_akao_sfx_control.paused_mask = kind == KEYED_AS_PAUSED_EFFECT ? 1u << test_voice : 0;
        g_akao_xa_tracker.voice_mask = kind == KEYED_AS_SAMPLED_PROGRAM ? 1 << test_voice : 0;
        registers[key_on_register / 2] = (uint16_t)(1u << test_voice);
        lom_native_akao_enable_event(0);
        Psyz_SpuPullSamples(out, FRAMES);
        int audible = 0;

        for (int i = 0; i < FRAMES * 2; ++i)
        {
            audible |= out[i];
        }

        failed |= (audible != 0) != (kind != KEYED_AS_MUSIC) || Psyz_SpuRead(voice_envelope_register) == 0;
    }

    lom_native_spu_set_category_volumes(full_volume, full_volume);
    g_akao_sfx_control = saved;
    g_akao_xa_tracker.voice_mask = saved_program;

    if (failed)
    {
        fprintf(stderr, probe_error);
    }
    else
    {
        puts(probe_success);
    }

    return failed ? -1 : 0;
}
