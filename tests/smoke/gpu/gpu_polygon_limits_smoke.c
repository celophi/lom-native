#include <libgpu.h>
#include <psyz/gpu.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Pack a vertex for a GP0 command.
 * @param x Across.
 * @param y Down.
 * @return The word.
 */
static uint32_t xy(int x, int y)
{
    return (uint16_t)x | ((uint32_t)(uint16_t)y << 16);
}

/**
 * @brief Send a GP0 command and wait for it to draw.
 * @param words The command.
 * @param count How many words.
 */
static void submit(const uint32_t* words, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
    {
        Psyz_GpuWriteGP0(words[i]);
    }

    Psyz_GpuExeque();
    DrawSync(0);
}

/**
 * @brief Clear the screen to black.
 */
static void clear(void)
{
    RECT r = {0, 0, 320, 240};
    ClearImage(&r, 0, 0, 0);
    DrawSync(0);
}

/**
 * @brief Check a pixel's color.
 * @param name The case, for the message.
 * @param x Across.
 * @param y Down.
 * @param expected Its 15-bit color.
 * @return 0, or 1 when it's wrong.
 */
static int check(const char* name, int x, int y, uint16_t expected)
{
    RECT r = {x, y, 2, 1};
    uint16_t pixels[2] = {0};

    if (StoreImage(&r, (u_long*)pixels) < 0 || (pixels[0] & 0x7FFF) != expected)
    {
        fprintf(stderr, "%s: pixel (%d,%d)=%04X expected %04X\n", name, x, y, pixels[0], expected);
        return 1;
    }

    return 0;
}

int main(void)
{
    DRAWENV draw;
    DISPENV display;
    int failed = 0;
    ResetGraph(0);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    draw.dtd = 0;
    SetDefDispEnv(&display, 0, 0, 320, 240);
    PutDrawEnv(&draw);
    PutDispEnv(&display);

    for (int reject = 0; reject < 2; reject++)
    {
        uint32_t wide[] = {0x20FFFFFF, xy(-reject, 0), xy(1023, 0), xy(100, 100)};
        uint32_t tall[] = {0x20FFFFFF, xy(0, -reject), xy(100, 511), xy(100, 0)};
        clear();
        submit(wide, 4);
        failed |= check(reject ? "width 1024 rejected" : "width 1023 accepted", 20, 20, reject ? 0 : 0x7FFF);
        clear();
        submit(tall, 4);
        failed |= check(reject ? "height 512 rejected" : "height 511 accepted", 80, 20, reject ? 0 : 0x7FFF);
    }

    uint32_t second[] = {0x28FFFFFF, xy(-1000, 0), xy(100, 0), xy(100, 100), xy(200, 100)};
    clear();
    submit(second, 5);
    failed |= check("bad first half", 50, 50, 0);
    failed |= check("good second half", 150, 80, 0x7FFF);
    uint32_t first[] = {0x28FFFFFF, xy(0, 0), xy(100, 0), xy(0, 100), xy(1023, 900)};
    clear();
    submit(first, 5);
    failed |= check("good first half", 20, 20, 0x7FFF);
    failed |= check("bad second half", 80, 60, 0);

    RECT tex = {640, 0, 2, 1};
    uint16_t white[2] = {0x7FFF, 0x7FFF};
    LoadImage(&tex, (u_long*)white);
    DrawSync(0);
    uint32_t wrapped[] = {0x2D808080, xy(1008, 20), 0, xy(1032, 20), 0x010A0000, xy(1008, 60), 0, xy(1032, 60), 0};
    clear();
    submit(wrapped, 9);
    failed |= check("wrapped textured quad", 20, 30, 0);
    uint32_t sprite[] = {0x65808080, xy(20, 80), 0, 0x00010002};
    submit(sprite, 4);
    failed |= check("rejected quad retains texture page", 20, 80, 0x7FFF);

    for (unsigned semi = 0; semi < 2; ++semi)
    {
        clear();
        Psyz_GpuWriteGP0(0xE1000020u);

        for (unsigned quad = 0; quad < 1024; ++quad)
        {
            int x = quad == 1023 ? 80 : 20;
            uint32_t color = quad == 1023 ? 0x0000FFu : 0xFF0000u;
            uint32_t words[] = {(semi ? 0x3A000000u : 0x38000000u) | color, xy(x, 20), color, xy(x + 16, 20), color, xy(x, 36), color, xy(x + 16, 36)};

            for (unsigned i = 0; i < 8; ++i)
            {
                Psyz_GpuWriteGP0(words[i]);
            }
        }

        uint32_t tail[] = {0x2800FF00u, xy(140, 20), xy(156, 20), xy(140, 36), xy(156, 36)};
        submit(tail, 5);
        failed |= check("Gouraud batch before overflow flush", 24, 24, 0x7C00);
        failed |= check("Gouraud quad at overflow boundary", 84, 24, 0x001F);
        failed |= check("command after Gouraud overflow boundary", 144, 24, 0x03E0);
    }

    static POLY_F4 queued[3278];
    clear();

    for (unsigned i = 0; i < 3278; ++i)
    {
        int x = i < 3276 ? 20 : i == 3276 ? 80 : 140;
        setPolyF4(&queued[i]);
        setRGB0(&queued[i], i < 3276 ? 255 : 0, i == 3276 ? 255 : 0, i == 3277 ? 255 : 0);
        setXY4(&queued[i], x, 80, x + 16, 80, x, 96, x + 16, 96);

        if (i + 1 < 3278)
        {
            setaddr(&queued[i], &queued[i + 1]);
        }
        else
        {
            termPrim(&queued[i]);
        }
    }

    DrawOTag((OT_TYPE*)queued);
    DrawSync(0);
    failed |= check("linked queue before flush", 24, 84, 0x001F);
    failed |= check("linked packet triggering flush", 84, 84, 0x03E0);
    failed |= check("linked packet following flush", 144, 84, 0x7C00);

    if (!failed)
    {
        puts("GPU polygon size limits, Gouraud overflow and linked-packet queue flush smoke passed.");
    }

    return failed;
}
