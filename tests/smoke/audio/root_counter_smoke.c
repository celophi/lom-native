extern void Psyz_RcntAdd(int n);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../support/lom_check.h"
extern long SetRCnt(long, unsigned short, long), GetRCnt(long), StartRCnt(long), StopRCnt(long), ResetRCnt(long);

extern void* InterruptCallback(int, void (*)());

extern long SetVideoMode(long);
#define C2 0xF2000002L
#define C3 0xF2000003L
;

static unsigned callbacks, callback_error, depth, max_depth, recursive_sample, c3_callbacks;

static long values[1024];

/**
 * @brief The root counter's callback: note the counter's value, check it
 *        doesn't move during the callback or the callback run inside itself,
 *        and step the counter once from inside when the case asks.
 */
static void tick(void)
{
    ++depth;

    if (depth > max_depth)
    {
        max_depth = depth;
    }

    long before = GetRCnt(C2);

    if (callbacks >= 1024)
    {
        callback_error = 1;
    }
    else
    {
        values[callbacks] = before;
    }

    ++callbacks;

    if (GetRCnt(C2) != before)
    {
        callback_error = 1;
    }

    if (recursive_sample)
    {
        recursive_sample = 0;
        Psyz_RcntAdd(1);
    }

    --depth;
}

/**
 * @brief Counter 3's callback, which counts its calls.
 */
static void tick3(void)
{
    ++c3_callbacks;
}

/**
 * @brief Set counter 2 up with a target and a mode, start it, and clear what
 *        the callback noted.
 * @param target The target.
 * @param mode The mode.
 * @return 0.
 */
static int setup(unsigned short target, long mode)
{
    CHECK_COUNTED(StopRCnt(C2) == 1);
    CHECK_COUNTED(SetRCnt(C2, target, mode) == 1);
    InterruptCallback(0, tick);
    CHECK_COUNTED(StartRCnt(C2) == 1);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(ResetRCnt(C2) == 1);
    callbacks = callback_error = depth = max_depth = recursive_sample = 0;
    memset(values, 0, sizeof(values));
    return 0;
}

int main(void)
{
    CHECK_COUNTED(SetRCnt(C2, 17640, 0x1000) == 1);
    InterruptCallback(0, tick);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(GetRCnt(C2) == 96 && !callbacks);
    Psyz_RcntAdd(182);
    CHECK_COUNTED(GetRCnt(C2) == 17568 && !callbacks);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(GetRCnt(C2) == 24 && !callbacks);
    CHECK_COUNTED(StartRCnt(C2) == 1 && !callbacks && GetRCnt(C2) == 24);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(callbacks == 1 && values[0] == 120 && GetRCnt(C2) == 120);
    CHECK_COUNTED(StopRCnt(C2) == 1 && GetRCnt(C2) == 120);
    Psyz_RcntAdd(368);
    CHECK_COUNTED(callbacks == 1 && GetRCnt(C2) == 168);
    CHECK_COUNTED(StartRCnt(C2) == 1 && callbacks == 1 && GetRCnt(C2) == 168);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(callbacks == 2 && values[1] == 264);
    CHECK_COUNTED(!callback_error);

    CHECK_COUNTED(StopRCnt(C2) == 1);
    Psyz_RcntAdd(184);
    CHECK_COUNTED(GetRCnt(C2) == 288);
    CHECK_COUNTED(InterruptCallback(0, NULL) == (void*)tick);
    CHECK_COUNTED(StartRCnt(C2) == 1);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(callbacks == 2 && GetRCnt(C2) == 384);
    CHECK_COUNTED(StopRCnt(C2) == 1 && InterruptCallback(0, tick) == NULL);
    CHECK_COUNTED(StartRCnt(C2) == 1 && callbacks == 2);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(callbacks == 3 && values[2] == 480);
    CHECK_COUNTED(ResetRCnt(C2) == 1 && GetRCnt(C2) == 0);
    Psyz_RcntAdd(184);
    CHECK_COUNTED(callbacks == 4 && values[3] == 24);
    CHECK_COUNTED(StopRCnt(C2) == 1);
    Psyz_RcntAdd(184);
    CHECK_COUNTED(ResetRCnt(C2) == 1 && StartRCnt(C2) == 1 && callbacks == 4);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(callbacks == 5 && values[4] == 96);

    CHECK_COUNTED(!setup(17640, 0x1000));
    Psyz_RcntAdd(10000);
    long bulk_count = GetRCnt(C2), bulk_values[1024];
    unsigned bulk_callbacks = callbacks;
    memcpy(bulk_values, values, sizeof(values));
    CHECK_COUNTED(bulk_callbacks == 54 && bulk_count == 7440);
    CHECK_COUNTED(!setup(17640, 0x1000));

    for (unsigned i = 0; i < 10000; i++)
    {
        Psyz_RcntAdd(1);
    }

    CHECK_COUNTED(GetRCnt(C2) == bulk_count && callbacks == bulk_callbacks);
    CHECK_COUNTED(!memcmp(values, bulk_values, sizeof(values)) && !callback_error && max_depth == 1);
    CHECK_COUNTED(values[0] == 24 && values[1] == 48 && values[2] == 72 && values[3] == 0);
    CHECK_COUNTED(!setup(17640, 0x1000));
    Psyz_RcntAdd(44100);
    CHECK_COUNTED(callbacks == 240 && GetRCnt(C2) == 0);

    CHECK_COUNTED(!setup(17640, 0));
    Psyz_RcntAdd(1000);
    CHECK_COUNTED(GetRCnt(C2) == 7800 && !callbacks);
    CHECK_COUNTED(!setup(0, 0x1000));
    Psyz_RcntAdd(682);
    CHECK_COUNTED(GetRCnt(C2) == 65472 && !callbacks);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(GetRCnt(C2) == 32 && callbacks == 1);
    CHECK_COUNTED(!setup(0, 0x1001));
    Psyz_RcntAdd(85);
    CHECK_COUNTED(GetRCnt(C2) == 65280 && !callbacks);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(GetRCnt(C2) == 512 && callbacks == 1);

    long unchanged = GetRCnt(C2);
    const long invalid[] = {0, 1, C3, 0xF2000102L, -1};

    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
    {
        CHECK_COUNTED(GetRCnt(invalid[i]) == 0 && StartRCnt(invalid[i]) == 0 && StopRCnt(invalid[i]) == 0);

        if (invalid[i] != C3)
        {
            CHECK_COUNTED(SetRCnt(invalid[i], 17640, 0x1000) == 0 && ResetRCnt(invalid[i]) == 0);
        }

        CHECK_COUNTED(GetRCnt(C2) == unchanged);
    }

    Psyz_RcntAdd(0);
    Psyz_RcntAdd(-1);
    CHECK_COUNTED(GetRCnt(C2) == unchanged);

    CHECK_COUNTED(!setup(96, 0x1000));
    recursive_sample = 1;
    Psyz_RcntAdd(1);
    CHECK_COUNTED(callbacks == 1 && max_depth == 1 && GetRCnt(C2) == 0 && !callback_error);
    Psyz_RcntAdd(1);
    CHECK_COUNTED(callbacks == 2 && max_depth == 1 && !callback_error);
    CHECK_COUNTED(StopRCnt(C2) == 1);
    CHECK_COUNTED(InterruptCallback(0, NULL) == (void*)tick);
    SetVideoMode(0);
    CHECK_COUNTED(SetRCnt(C3, 123, 0) == 1);
    InterruptCallback(0, tick3);
    Psyz_RcntAdd(44100);
    CHECK_COUNTED(c3_callbacks == 60 && GetRCnt(C3) == 0);
    CHECK_COUNTED(ResetRCnt(C3) == 1);
    Psyz_RcntAdd(22050);
    CHECK_COUNTED(c3_callbacks == 90);
    SetVideoMode(1);
    CHECK_COUNTED(SetRCnt(C3, 0, 0) == 1);
    Psyz_RcntAdd(44100);
    CHECK_COUNTED(c3_callbacks == 140);
    SetVideoMode(0);

    if (!lom_check_failures)
    {
        puts(
            "Root counter: exact16-bit sample clock, target/mode, phase-preserving masks, latched IRQ, NULL removal, split equivalence and counter3 preserved");
    }

    return lom_check_summary("Root counter");
}
