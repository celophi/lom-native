#include "main/cdrom.h"

#define u_long LomPsyzULong
#include <libcd.h>
#undef u_long

void cdrom_set_audio_volume(u8 volume, s32 mix_mode)
{
    CdlATV audio_mix;

    if (mix_mode != 0)
    {
        audio_mix.val0 = volume;
        audio_mix.val1 = 0;
        audio_mix.val2 = volume;
        audio_mix.val3 = 0;
    }
    else
    {
        audio_mix.val0 = volume;
        audio_mix.val1 = volume;
        audio_mix.val2 = 0;
        audio_mix.val3 = 0;
    }

    CdMix(&audio_mix);
}
