#define u_long LomPsyzULong
#include <psyz/gte.h>
#undef u_long
#include <stdint.h>
#include <stdio.h>
#include "../../support/lom_check.h"
#ifndef __psyz
#error "The regression must call the normal SDK command bridge, not MIPS asm"
#endif
;

/**
 * @brief Load the GTE's three screen XY registers.
 * @param a SXY0.
 * @param b SXY1.
 * @param c SXY2.
 */
static void xy(uint32_t a, uint32_t b, uint32_t c)
{
    Psyz_GteDataWrite(12, a);
    Psyz_GteDataWrite(13, b);
    Psyz_GteDataWrite(14, c);
}

int main(void)
{
    uint32_t stored = 0;
    xy(0x00010002, 0x00030004, 0x00050006);
    CHECK_COUNTED(Psyz_GteDataRead(15) == 0x00050006);
    Psyz_GteStsxy(&stored);
    CHECK_COUNTED(stored == 0x00050006);
    Psyz_GteDataWrite(15, 0x00070008);
    CHECK_COUNTED(Psyz_GteDataRead(12) == 0x00030004);
    CHECK_COUNTED(Psyz_GteDataRead(13) == 0x00050006);
    CHECK_COUNTED(Psyz_GteDataRead(14) == 0x00070008);
    CHECK_COUNTED(Psyz_GteDataRead(15) == 0x00070008);
    Psyz_GteDataWrite(15, 0x11111111);
    Psyz_GteDataWrite(15, 0x22222222);
    Psyz_GteDataWrite(15, 0x33333333);
    CHECK_COUNTED(Psyz_GteDataRead(12) == 0x11111111);
    CHECK_COUNTED(Psyz_GteDataRead(13) == 0x22222222);
    CHECK_COUNTED(Psyz_GteDataRead(14) == 0x33333333);
    CHECK_COUNTED(Psyz_GteDataRead(15) == 0x33333333);
    Psyz_GteDataWrite(14, 0xaabbccdd);
    CHECK_COUNTED(Psyz_GteDataRead(15) == 0xaabbccdd);
    Psyz_GteStsxy(&stored);
    CHECK_COUNTED(stored == 0xaabbccdd);
    xy(0x22223333, 0x00000000, 0x00000001);
    Psyz_GteDataWrite(15, 0x00010000);
    Psyz_GteCommand(0x1400006u);
    CHECK_COUNTED(Psyz_GteDataRead(24) == 1);
    CHECK_COUNTED(Psyz_GteCtrlRead(31) == 0);

    static const struct
    {
        uint32_t input, expected;
    } h[] = {{0, 0}, {0x7fff, 0x7fff}, {0x8000, 0xffff8000}, {0xffff, 0xffffffff}, {0x12348000, 0xffff8000}, {0xffff0001, 1}};

    for (unsigned i = 0; i < sizeof h / sizeof h[0]; ++i)
    {
        Psyz_GteCtrlWrite(26, h[i].input);
        CHECK_COUNTED(Psyz_GteCtrlRead(26) == h[i].expected);
    }

    for (unsigned i = 0; i < 32; ++i)
    {
        Psyz_GteCtrlWrite(i, 0);
    }

    Psyz_GteCtrlWrite(0, 0x1000);
    Psyz_GteCtrlWrite(2, 0x1000);
    Psyz_GteCtrlWrite(4, 0x1000);
    Psyz_GteCtrlWrite(26, 0x1234fffe);
    Psyz_GteDataWrite(0, 1);
    Psyz_GteDataWrite(1, 0x7fff);
    Psyz_GteCommand(0x0180001u);
    CHECK_COUNTED(Psyz_GteCtrlRead(26) == 0xfffffffe);
    CHECK_COUNTED(Psyz_GteDataRead(14) == 1);
    CHECK_COUNTED(Psyz_GteDataRead(19) == 0x7fff);
    CHECK_COUNTED(Psyz_GteDataRead(25) == 1);
    CHECK_COUNTED(Psyz_GteCtrlRead(31) == 0x80020000);
    static const unsigned signed_control[] = {4, 12, 20, 27, 29, 30};

    for (unsigned i = 0; i < sizeof signed_control / sizeof signed_control[0]; ++i)
    {
        Psyz_GteCtrlWrite(signed_control[i], 0xffff8000);
        CHECK_COUNTED(Psyz_GteCtrlRead(signed_control[i]) == 0xffff8000);
        Psyz_GteCtrlWrite(signed_control[i], 0xdead7fff);
        CHECK_COUNTED(Psyz_GteCtrlRead(signed_control[i]) == 0x00007fff);
    }

    static const unsigned signed_data[] = {1, 3, 5, 8, 9, 10, 11};

    for (unsigned i = 0; i < sizeof signed_data / sizeof signed_data[0]; ++i)
    {
        Psyz_GteDataWrite(signed_data[i], 0x12348000);
        CHECK_COUNTED(Psyz_GteDataRead(signed_data[i]) == 0xffff8000);
    }

    static const unsigned unsigned_data[] = {7, 16, 17, 18, 19};

    for (unsigned i = 0; i < sizeof unsigned_data / sizeof unsigned_data[0]; ++i)
    {
        Psyz_GteDataWrite(unsigned_data[i], 0xdeadbeef);
        CHECK_COUNTED(Psyz_GteDataRead(unsigned_data[i]) == 0x0000beef);
    }

    static const struct
    {
        uint32_t input, expected;
    } flags[] = {{0xffffffff, 0xfffff000}, {0x80000000, 0},          {0x00000fff, 0},          {0x00001000, 0x00001000},
                 {0x00400000, 0x00400000}, {0x00002000, 0x80002000}, {0x40000000, 0xc0000000}, {0x80001000, 0x00001000}};

    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; ++i)
    {
        Psyz_GteCtrlWrite(31, flags[i].input);
        CHECK_COUNTED(Psyz_GteCtrlRead(31) == flags[i].expected);
    }

    for (unsigned bit = 0; bit < 32; ++bit)
    {
        uint32_t expected = bit < 12 || bit == 31 ? 0 : UINT32_C(1) << bit;

        if ((bit >= 13 && bit <= 18) || (bit >= 23 && bit <= 30))
        {
            expected |= 0x80000000u;
        }

        Psyz_GteCtrlWrite(31, UINT32_C(1) << bit);
        CHECK_COUNTED(Psyz_GteCtrlRead(31) == expected);
    }

    Psyz_GteCtrlWrite(31, 0x00002000);
    Psyz_GteDataWrite(9, 0x1234);
    Psyz_GteDataWrite(10, 0xfffffffd);
    Psyz_GteDataWrite(11, 0x7ff);
    Psyz_GteDataWrite(25, 0x12345678);
    Psyz_GteDataWrite(26, 0xdeadbeef);
    Psyz_GteDataWrite(27, 0x76543210);
    Psyz_GteDataWrite(30, 0x13579bdf);
    uint32_t before[32], control[32];

    for (unsigned i = 0; i < 32; ++i)
    {
        if (i != 15)
        {
            before[i] = Psyz_GteDataRead(i);
        }
    }

    for (unsigned i = 0; i < 32; ++i)
    {
        control[i] = Psyz_GteCtrlRead(i);
    }

    (void)Psyz_GteDataRead(15);
    Psyz_GteDataWrite(15, 0x8020ffe0);

    for (unsigned i = 0; i < 32; ++i)
    {
        if (i < 12 || i > 15)
        {
            CHECK_COUNTED(Psyz_GteDataRead(i) == before[i]);
        }
    }

    for (unsigned i = 0; i < 32; ++i)
    {
        CHECK_COUNTED(Psyz_GteCtrlRead(i) == control[i]);
    }

    Psyz_GteCtrlWrite(31, 0x00002000);

    for (unsigned i = 0; i < 32; ++i)
    {
        if (i < 12 || i > 15)
        {
            CHECK_COUNTED(Psyz_GteDataRead(i) == before[i]);
        }
    }

    for (unsigned i = 0; i < 31; ++i)
    {
        CHECK_COUNTED(Psyz_GteCtrlRead(i) == control[i]);
    }

    return lom_check_summary("GTE register transfers");
}
