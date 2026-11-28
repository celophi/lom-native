#include "main/cdrom.h"
#define u_long LomPsyzULong
#include <libcd.h>
#undef u_long
#include <stddef.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(CdlATV) == 4, "CD attenuation has four bytes");

_Static_assert(offsetof(CdlATV, val0) == 0 && offsetof(CdlATV, val1) == 1 && offsetof(CdlATV, val2) == 2 && offsetof(CdlATV, val3) == 3,
               "CD attenuation field order follows original stores");

static unsigned mix_calls, volume_calls, wrapper_errors, phase;

static unsigned char at_mix[4], at_volume[4];

extern int __real_CdMix(CdlATV* value);

/**
 * @brief Note the CD audio volume CdMix sets, which must come after the mix.
 * @param value The four volumes.
 * @return -73, which CdMix must not pass on.
 */
int __wrap_CD_vol(CdlATV* value)
{
    if (phase != 2)
    {
        ++wrapper_errors;
    }

    ++volume_calls;
    memcpy(at_volume, value, sizeof(at_volume));
    phase = 3;
    return -73;
}

/**
 * @brief Note the CD audio mix the game sets, then check the real CdMix sets
 *        the volume once and returns 1 whatever CD_vol says.
 * @param value The four volumes.
 * @return What the real one returns.
 */
int __wrap_CdMix(CdlATV* value)
{
    if (phase != 1)
    {
        ++wrapper_errors;
    }

    ++mix_calls;
    memcpy(at_mix, value, sizeof(at_mix));
    phase = 2;
    unsigned before = volume_calls;
    int result = __real_CdMix(value);

    if (phase != 3 || volume_calls != before + 1 || result != 1)
    {
        ++wrapper_errors;
    }

    phase = 4;
    return result;
}

int main(void)
{
    const s32 modes[] = {-17, 0, 1, 17};
    unsigned failed = 0;

    for (unsigned volume = 0; volume < 256; ++volume)
    {
        for (unsigned m = 0; m < sizeof(modes) / sizeof(modes[0]); ++m)
        {
            const unsigned char expected[4] = {(unsigned char)volume, modes[m] == 0 ? (unsigned char)volume : 0, modes[m] != 0 ? (unsigned char)volume : 0, 0};
            unsigned before_mix = mix_calls, before_volume = volume_calls;
            memset(at_mix, 0xA5, sizeof(at_mix));
            memset(at_volume, 0x5A, sizeof(at_volume));
            phase = 1;
            cdrom_set_audio_volume((u8)volume, modes[m]);

            if (phase != 4 || mix_calls != before_mix + 1 || volume_calls != before_volume + 1 || wrapper_errors ||
                memcmp(at_mix, expected, sizeof(expected)) || memcmp(at_volume, expected, sizeof(expected)))
            {
                if (!failed)
                {
                    fprintf(stderr, "CD matrix volume=%u mode=%d: calls=%u/%u phase=%u errors=%u; original forwarding missing or changed\n", volume, modes[m],
                            mix_calls - before_mix, volume_calls - before_volume, phase, wrapper_errors);
                }

                ++failed;
            }
        }
    }

    if (failed)
    {
        fprintf(stderr, "CD matrix failures: %u/1024\n", failed);
        return 1;
    }

    puts("Original CD attenuation setter -> real CdMix -> CD_vol passed: 1024 cases.");
    return 0;
}
