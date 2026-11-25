#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void Psyz_AudioSetVirtualClock(int enabled) __attribute__((weak));

int Psyz_VideoSetVsyncMode(int mode) __attribute__((weak));

#define LOM_TEST_PSYZ_VSYNC_LIMITLESS 3

/**
 * @brief Before main, switch to the virtual clock when LOM_TEST_CLOCK is
 *        virtual, so tests run as fast as they can without changing what
 *        happens. Any value other than paced or virtual stops the test.
 */
__attribute__((constructor)) void lom_native_test_clock_init(void)
{
    const char* clock = getenv("LOM_TEST_CLOCK");

    if (clock == NULL || clock[0] == '\0' || strcmp(clock, "paced") == 0)
    {
        return;
    }

    if (strcmp(clock, "virtual") != 0)
    {
        fprintf(stderr, "[lom-native] unknown LOM_TEST_CLOCK=%s (use paced or virtual)\n", clock);
        _exit(2);
    }

    if (Psyz_AudioSetVirtualClock == NULL || Psyz_VideoSetVsyncMode == NULL)
    {
        return;
    }

    Psyz_AudioSetVirtualClock(1);
    Psyz_VideoSetVsyncMode(LOM_TEST_PSYZ_VSYNC_LIMITLESS);
    fprintf(stderr, "[lom-native] test clock: virtual\n");
}
