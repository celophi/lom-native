#include <psyz/gte.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../support/lom_check.h"
#ifndef __psyz
#error "CPU smoke requires the actual native Psyz_GteCommand endpoint"
#endif
#define TEST_SQR 0x00a00428u
#define TEST_GPF 0x0190003du
_Static_assert((TEST_SQR & 63u) == 0x28u && ((TEST_SQR >> 19) & 1u) == 0 && ((TEST_SQR >> 10) & 1u) == 1, "SQR sf0/lm1 command");

_Static_assert((TEST_GPF & 63u) == 0x3du && ((TEST_GPF >> 19) & 1u) == 0 && ((TEST_GPF >> 10) & 1u) == 0, "GPF sf0/lm0 command");

;

static const struct
{
    uint32_t ir[3];
    unsigned packed;
} cases[] = {
    {{0x0f80, 0x0f80, 0x0f80}, 0x7fff},
    {{0xffff8000, 0x2000, 0x380}, 0x1fe0},
    {{0x1000, 0x1000, 0x1000}, 0x7fff},
    {{0, 127, 128}, 0x0400},
    {{128, 255, 256}, 0x0821},
    {{3967, 3968, 4095}, 0x7ffe},
    {{0xffff7fff, 0xffff0001, 0xffff}, 0x001f},
    {{0xffff007f, 0xdead0080, 0xffff0180}, 0x0c20},
    {{0xffff, 0xffff8000, 0x7fff}, 0x7c00},
    {{0x80000000, 0x7fff8000, 0xa5a5ffff}, 0},
};

static const unsigned expanded[32] = {0,    128,  256,  384,  512,  640,  768,  896,  1024, 1152, 1280, 1408, 1536, 1664, 1792, 1920,
                                      2048, 2176, 2304, 2432, 2560, 2688, 2816, 2944, 3072, 3200, 3328, 3456, 3584, 3712, 3840, 3968};

/**
 * @brief Read every GTE register but the two color conversion ones, which read
 *        differently from how they're written.
 * @param data Set to the data registers.
 * @param control Set to the control registers.
 */
static void snapshot(unsigned data[32], unsigned control[32])
{
    for (unsigned i = 0; i < 32; ++i)
    {
        if (i != 28 && i != 29)
        {
            data[i] = Psyz_GteDataRead(i);
        }
    }

    for (unsigned i = 0; i < 32; ++i)
    {
        control[i] = Psyz_GteCtrlRead(i);
    }

    data[28] = Psyz_GteDataRead(28);
    data[29] = Psyz_GteDataRead(29);
}

/**
 * @brief Check the GTE's registers are as they were.
 * @param data The data registers before.
 * @param control The control registers before.
 * @param ir_changes Nonzero when IR1 to IR3, and the two that mirror them, may
 *        have changed.
 */
static void unchanged(const unsigned data[32], const unsigned control[32], int ir_changes)
{
    for (unsigned i = 0; i < 32; ++i)
    {
        if (!ir_changes || !((i >= 9 && i <= 11) || i == 28 || i == 29))
        {
            CHECK_COUNTED(Psyz_GteDataRead(i) == data[i]);
        }
    }

    for (unsigned i = 0; i < 32; ++i)
    {
        CHECK_COUNTED(Psyz_GteCtrlRead(i) == control[i]);
    }
}

int main(void)
{
    unsigned before[32], control[32];

    for (unsigned i = 0; i < 28; ++i)
    {
        Psyz_GteDataWrite(i, 0x23456789u ^ (i * 0x01010101u));
    }

    Psyz_GteDataWrite(30, 0x00008000u);

    for (unsigned i = 0; i < 32; ++i)
    {
        Psyz_GteCtrlWrite(i, 0x13579bdfu ^ (i * 0x01010101u));
    }

    for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; ++c)
    {
        for (unsigned i = 0; i < 3; ++i)
        {
            Psyz_GteDataWrite(9 + i, cases[c].ir[i]);
        }

        snapshot(before, control);
        CHECK_COUNTED(Psyz_GteDataRead(28) == cases[c].packed);
        CHECK_COUNTED(Psyz_GteDataRead(29) == cases[c].packed);
        unchanged(before, control, 0);
        Psyz_GteDataWrite(29, ~cases[c].packed);
        unchanged(before, control, 0);
    }

    static const struct
    {
        unsigned word, ir[3], packed;
    } writes[] = {{0x7fff, {0x0f80, 0x0f80, 0x0f80}, 0x7fff}, {0x000a, {0x0500, 0, 0}, 0x000a}, {0xffff000a, {0x0500, 0, 0}, 0x000a}, {0, {0, 0, 0}, 0}};

    for (unsigned w = 0; w < sizeof writes / sizeof writes[0]; ++w)
    {
        snapshot(before, control);
        Psyz_GteDataWrite(28, writes[w].word);

        for (unsigned i = 0; i < 3; ++i)
        {
            CHECK_COUNTED(Psyz_GteDataRead(9 + i) == writes[w].ir[i]);
        }

        CHECK_COUNTED(Psyz_GteDataRead(28) == writes[w].packed);
        CHECK_COUNTED(Psyz_GteDataRead(29) == writes[w].packed);
        unchanged(before, control, 1);
    }

    for (unsigned x = 0; x < 32768; ++x)
    {
        snapshot(before, control);
        Psyz_GteDataWrite(28, x | 0xffff8000u);
        CHECK_COUNTED(Psyz_GteDataRead(9) == expanded[x % 32]);
        CHECK_COUNTED(Psyz_GteDataRead(10) == expanded[(x / 32) % 32]);
        CHECK_COUNTED(Psyz_GteDataRead(11) == expanded[(x / 1024) % 32]);
        CHECK_COUNTED(Psyz_GteDataRead(28) == x && Psyz_GteDataRead(29) == x);
        unchanged(before, control, 1);
    }

    for (unsigned axis = 0; axis < 3; ++axis)
    {
        for (unsigned raw = 0; raw < 65536; ++raw)
        {
            unsigned expected = 0;

            if (raw < 32768)
            {
                for (unsigned band = 1; band < 32; ++band)
                {
                    if (raw >= expanded[band])
                    {
                        expected = band;
                    }
                }
            }

            Psyz_GteDataWrite(9, 0x0080);
            Psyz_GteDataWrite(10, 0x0100);
            Psyz_GteDataWrite(11, 0x0180);
            Psyz_GteDataWrite(9 + axis, raw);
            unsigned full = (1u | (2u << 5) | (3u << 10));
            full = (full & ~(31u << (5 * axis))) | (expected << (5 * axis));
            CHECK_COUNTED(Psyz_GteDataRead(28) == full);
            CHECK_COUNTED(Psyz_GteDataRead(29) == full);
        }
    }

    Psyz_GteDataWrite(28, 0x7fff);
    Psyz_GteDataWrite(9, 1);
    Psyz_GteDataWrite(10, 16);
    Psyz_GteDataWrite(11, 64);
    Psyz_GteCommand(TEST_SQR);
    CHECK_COUNTED(Psyz_GteDataRead(9) == 1 && Psyz_GteDataRead(10) == 256 && Psyz_GteDataRead(11) == 4096);
    snapshot(before, control);
    CHECK_COUNTED(Psyz_GteDataRead(28) == 0x7c40 && Psyz_GteDataRead(29) == 0x7c40);
    unchanged(before, control, 0);
    Psyz_GteDataWrite(29, 0xffffffffu);
    unchanged(before, control, 0);
    Psyz_GteDataWrite(8, 2);
    Psyz_GteDataWrite(9, 0xffffffc0u);
    Psyz_GteDataWrite(10, 2048);
    Psyz_GteDataWrite(11, 32767);
    Psyz_GteCommand(TEST_GPF);
    CHECK_COUNTED(Psyz_GteDataRead(9) == 0xffffff80u);
    CHECK_COUNTED(Psyz_GteDataRead(10) == 4096 && Psyz_GteDataRead(11) == 32767);
    CHECK_COUNTED(Psyz_GteCtrlRead(31) != 0);
    snapshot(before, control);
    CHECK_COUNTED(Psyz_GteDataRead(28) == 0x7fe0 && Psyz_GteDataRead(29) == 0x7fe0);
    unchanged(before, control, 0);
    Psyz_GteDataWrite(29, 0x12345678u);
    unchanged(before, control, 0);
    Psyz_GteDataWrite(28, 0x0123u);
    unchanged(before, control, 1);
    snapshot(before, control);
    CHECK_COUNTED(Psyz_GteDataRead(32) == 0 && Psyz_GteDataRead(0xffffffffu) == 0);
    Psyz_GteDataWrite(32, 0x12345678);
    Psyz_GteDataWrite(0xffffffffu, 0x12345678);
    unchanged(before, control, 0);
    return lom_check_summary("GTE color aliases");
}
