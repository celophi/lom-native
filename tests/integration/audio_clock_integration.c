#include <psyz/audio.h>

#include <stdio.h>
#include <time.h>

static volatile unsigned s_tick_count;

extern long SetRCnt(long spec, unsigned short target, long mode);

extern long StartRCnt(long spec);

extern void* InterruptCallback(int source, void (*callback)());

/**
 * @brief A root counter callback that counts its calls.
 */
static void count_tick(void)
{
    s_tick_count++;
}

/**
 * @brief Verify that host audio sample pulls advance the PS1 root-counter shim.
 * @return Zero when the audio device opens and counter 2 produces callbacks.
 */
int main(void)
{
    struct timespec sleep_time;
    unsigned i;

    if (SetRCnt(0xF2000002, 0x44E8, 0x1000) == 0)
    {
        fprintf(stderr, "Failed to configure PS1 root counter 2.\n");
        return 1;
    }

    InterruptCallback(0, count_tick);

    if (StartRCnt(0xF2000002) == 0)
    {
        fprintf(stderr, "Failed to enable root counter 2 interrupts.\n");
        return 1;
    }

    if (Psyz_AudioInit() != 0)
    {
        fprintf(stderr, "Failed to initialize PSY-Z host audio.\n");
        return 2;
    }

    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 10000000;

    for (i = 0; i < 100 && s_tick_count == 0; i++)
    {
        nanosleep(&sleep_time, NULL);
    }

    Psyz_AudioDestroy();

    if (s_tick_count == 0)
    {
        fprintf(stderr, "PSY-Z audio opened but produced no root-counter ticks.\n");
        return 3;
    }

    printf("LoM audio clock smoke passed.\n");
    printf("  root-counter ticks: %u\n", s_tick_count);
    return 0;
}
