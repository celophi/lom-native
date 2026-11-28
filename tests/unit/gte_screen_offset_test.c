#define u_long LomPsyzULong
#include <libgte.h>
#include <psyz/gte.h>
#undef u_long
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../support/lom_check.h"
#ifndef __psyz
#error "Requires actual normal SDK command bridge"
#endif
;

/**
 * @brief Read every GTE register but the ones that read differently from how
 *        they're written.
 * @param d Set to the data registers.
 * @param c Set to the control registers.
 */
static void snapshot(uint32_t d[32], uint32_t c[32])
{
    for (unsigned i = 0; i < 32; ++i)
    {
        if (i != 15 && i != 28 && i != 29)
        {
            d[i] = Psyz_GteDataRead(i);
        }
    }

    for (unsigned i = 0; i < 32; ++i)
    {
        c[i] = Psyz_GteCtrlRead(i);
    }

    d[15] = Psyz_GteDataRead(15);
    d[28] = Psyz_GteDataRead(28);
    d[29] = Psyz_GteDataRead(29);
}

/**
 * @brief Clear the GTE, then give it an identity rotation and a screen offset
 *        to project with.
 */
static void setup(void)
{
    for (unsigned i = 0; i < 32; ++i)
    {
        Psyz_GteCtrlWrite(i, 0);
        Psyz_GteDataWrite(i, 0);
    }

    Psyz_GteCtrlWrite(0, 0x1000);
    Psyz_GteCtrlWrite(2, 0x1000);
    Psyz_GteCtrlWrite(4, 0x1000);
    Psyz_GteCtrlWrite(26, 1);
}

int main(void)
{
    static const uint32_t patterns[] = {0, 1, 0xffff, 0x8000, 0xffff8000, 0x7fffffff, 0x80000000, 0xffffffff, 0x12345678, 0x89abcdef};
    setup();
    Psyz_GteDataWrite(9, 0xffff8000);
    Psyz_GteDataWrite(10, 0x1234);
    Psyz_GteDataWrite(24, 0x89abcdef);
    Psyz_GteDataWrite(30, 0x00f12345);
    Psyz_GteCtrlWrite(31, 0x80016000);

    for (unsigned axis = 24; axis <= 25; ++axis)
    {
        for (unsigned n = 0; n < sizeof(patterns) / sizeof(*patterns); ++n)
        {
            uint32_t before_d[32], before_c[32], after_d[32], after_c[32];
            snapshot(before_d, before_c);
            Psyz_GteCtrlWrite(axis, patterns[n]);
            snapshot(after_d, after_c);
            CHECK_COUNTED(after_c[axis] == patterns[n]);
            CHECK_COUNTED(!memcmp(before_d, after_d, sizeof(before_d)));
            before_c[axis] = patterns[n];
            CHECK_COUNTED(!memcmp(before_c, after_c, sizeof(before_c)));
        }
    }

    static const struct
    {
        long x, y;
        uint32_t rawx, rawy;
    } integer_api[] = {{160, 120, 0x00a00000, 0x00780000},
                       {-1, -32768, 0xffff0000, 0x80000000},
                       {32768, 65536, 0x80000000, 0},
                       {2147483647L, (-2147483647L - 1), 0xffff0000, 0}};

    for (unsigned n = 0; n < sizeof(integer_api) / sizeof(*integer_api); ++n)
    {
        SetGeomOffset(integer_api[n].x, integer_api[n].y);
        CHECK_COUNTED(Psyz_GteCtrlRead(24) == integer_api[n].rawx);
        CHECK_COUNTED(Psyz_GteCtrlRead(25) == integer_api[n].rawy);
    }

    static const struct
    {
        uint32_t xoff, yoff;
        int16_t x, y;
        uint32_t sxy, flag;
    } proj[] = {{0x8000, 0x8000, 1, -1, 0x00000001, 0},         {0xffff8000, 0xffff8000, 1, -1, 0xffff0000, 0}, {0x03ff8000, 0, 1, 0, 0x000003ff, 0x80004000},
                {0, 0x03ff8000, 0, 1, 0x03ff0000, 0x80002000},  {0x7fffffff, 0, 1, 0, 0x000003ff, 0x80014000},  {0, 0x7fffffff, 0, 1, 0x03ff0000, 0x80012000},
                {0x80008000, 0, -1, 0, 0x0000fc00, 0x80004000}, {0, 0x80008000, 0, -1, 0xfc000000, 0x80002000}, {0x80000000, 0, -1, 0, 0x0000fc00, 0x8000c000},
                {0x00a00000, 0x00780000, 1, -1, 0x007700a0, 0}};

    for (unsigned n = 0; n < sizeof(proj) / sizeof(*proj); ++n)
    {
        setup();
        Psyz_GteCtrlWrite(24, proj[n].xoff);
        Psyz_GteCtrlWrite(25, proj[n].yoff);
        Psyz_GteDataWrite(0, (uint16_t)proj[n].x | ((uint32_t)(uint16_t)proj[n].y << 16));
        Psyz_GteDataWrite(1, 2);
        Psyz_GteDataWrite(12, 0x00010002);
        Psyz_GteDataWrite(13, 0x00030004);
        Psyz_GteDataWrite(14, 0x00050006);
        Psyz_GteDataWrite(20, 0x11223344);
        Psyz_GteDataWrite(21, 0x55667788);
        Psyz_GteDataWrite(22, 0x99aabbcc);
        Psyz_GteCommand(0x0180001);
        CHECK_COUNTED(Psyz_GteDataRead(12) == 0x00030004);
        CHECK_COUNTED(Psyz_GteDataRead(13) == 0x00050006);
        CHECK_COUNTED(Psyz_GteDataRead(14) == proj[n].sxy);
        CHECK_COUNTED(Psyz_GteDataRead(15) == proj[n].sxy);
        CHECK_COUNTED(Psyz_GteCtrlRead(31) == proj[n].flag);
        CHECK_COUNTED(Psyz_GteCtrlRead(24) == proj[n].xoff);
        CHECK_COUNTED(Psyz_GteCtrlRead(25) == proj[n].yoff);
        CHECK_COUNTED(Psyz_GteDataRead(19) == 2);
        CHECK_COUNTED(Psyz_GteDataRead(24) == 0);
        CHECK_COUNTED(Psyz_GteDataRead(8) == 0);
        CHECK_COUNTED(Psyz_GteDataRead(25) == (uint32_t)(int32_t)proj[n].x);
        CHECK_COUNTED(Psyz_GteDataRead(26) == (uint32_t)(int32_t)proj[n].y);
        CHECK_COUNTED(Psyz_GteDataRead(27) == 2);
        CHECK_COUNTED(Psyz_GteDataRead(20) == 0x11223344);
        CHECK_COUNTED(Psyz_GteDataRead(21) == 0x55667788);
        CHECK_COUNTED(Psyz_GteDataRead(22) == 0x99aabbcc);
    }

    setup();
    Psyz_GteCtrlWrite(24, 0x8000);
    Psyz_GteCtrlWrite(25, 0x8000);
    Psyz_GteDataWrite(0, 0xffff0001);
    Psyz_GteDataWrite(1, 2);
    Psyz_GteDataWrite(2, 0x0001ffff);
    Psyz_GteDataWrite(3, 2);
    Psyz_GteDataWrite(4, 0xfffd0003);
    Psyz_GteDataWrite(5, 2);
    Psyz_GteCommand(0x0280030);
    CHECK_COUNTED(Psyz_GteDataRead(12) == 0x00000001);
    CHECK_COUNTED(Psyz_GteDataRead(13) == 0x00010000);
    CHECK_COUNTED(Psyz_GteDataRead(14) == 0xffff0002);
    CHECK_COUNTED(Psyz_GteDataRead(15) == 0xffff0002);
    CHECK_COUNTED(Psyz_GteCtrlRead(31) == 0);
    CHECK_COUNTED(Psyz_GteCtrlRead(24) == 0x8000);
    CHECK_COUNTED(Psyz_GteCtrlRead(25) == 0x8000);
    CHECK_COUNTED(Psyz_GteDataRead(25) == 3);
    CHECK_COUNTED(Psyz_GteDataRead(26) == 0xfffffffd);
    CHECK_COUNTED(Psyz_GteDataRead(27) == 2);
    return lom_check_summary("GTE screen offsets");
}
