#include "movie_audio_original_voice_oracle.h"

#include "main/audio/internal/akao_driver.h"
#include <stdint.h>
AkaoDriverFlags g_akao_driver_flags;

AkaoXaTracker g_akao_xa_tracker;

s16 g_akao_pan_gain_table[256];

static int gain_left[2], gain_right[2];

void spu_set_voice_volume(s32 voice, u32 l, u32 r, s32 scale)
{
    (void)scale;
    gain_left[voice] = l;
    gain_right[voice] = r;
}

void spu_set_voice_pitch(s32 v, s32 p)
{
    (void)v;
    (void)p;
}

void spu_set_voice_start_addr(s32 v, u32 a)
{
    (void)v;
    (void)a;
}

void spu_set_voice_repeat_addr(s32 v, u32 a)
{
    (void)v;
    (void)a;
}

void spu_set_voice_attack(s32 v, s32 a, u32 b)
{
    (void)v;
    (void)a;
    (void)b;
}

void spu_set_voice_decay_shift(s32 v, s32 a)
{
    (void)v;
    (void)a;
}

void spu_set_voice_sustain_level(s32 v, s32 a)
{
    (void)v;
    (void)a;
}

void spu_set_voice_sustain_mode(s32 v, s32 a, u32 b)
{
    (void)v;
    (void)a;
    (void)b;
}

void spu_set_voice_release_mode(s32 v, s32 a, u32 b)
{
    (void)v;
    (void)a;
    (void)b;
}

extern void akao_xa_setup_voice(s32, s32, u32, u32);

void original_movie_gains(int mono, int16_t center, unsigned volume, int gains[4])
{
    g_akao_driver_flags.output_mode = mono ? 2 : 1;
    g_akao_pan_gain_table[128] = center;
    g_akao_xa_tracker.volume = volume;
    akao_xa_setup_voice(0, 1, 0x1100, 0x2100);
    akao_xa_setup_voice(1, 2, 0x1900, 0x2900);
    gains[0] = gain_left[0];
    gains[1] = gain_right[0];
    gains[2] = gain_left[1];
    gains[3] = gain_right[1];
}

extern void akao_xa_cmd_set_volume(AkaoCommandParam*);

void original_skip_gains(unsigned volume, int gains[4])
{
    AkaoCommandParam params[1] = {0};
    params[0].value = volume;
    g_akao_xa_tracker.first_voice = 0;
    g_akao_xa_tracker.voice_mask = 3;
    akao_xa_cmd_set_volume(params);
    gains[0] = gain_left[0];
    gains[1] = gain_right[0];
    gains[2] = gain_left[1];
    gains[3] = gain_right[1];
}
