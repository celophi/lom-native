#include <libgpu.h>
#include <psyz/gpu.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Send a GP0 command.
 * @param words The command.
 * @param count How many words.
 */
static void submit(const uint32_t* words, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
    {
        Psyz_GpuWriteGP0(words[i]);
    }

    Psyz_GpuExeque();
}

int main(void)
{
    DRAWENV draw;
    DISPENV display;
    RECT clear = {0, 0, 1024, 512};
    RECT source = {640, 0, 2, 1};
    uint16_t green[2] = {0x03E0, 0x03E0}, red[2] = {0x001F, 0x001F};
    ResetGraph(0);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    SetDefDispEnv(&display, 0, 0, 320, 240);
    PutDrawEnv(&draw);
    PutDispEnv(&display);
    ClearImage(&clear, 0, 0, 0);
    LoadImage(&source, (u_long*)green);
    source.x = 704;
    LoadImage(&source, (u_long*)red);
    DrawSync(0);

    for (unsigned gouraud = 0; gouraud < 2; ++gouraud)
    {
        uint32_t mode = 0xE100010A;
        const uint32_t flat[] = {0x24808080, 400, 0, 400, 0x010B0000, 400, 0};
        const uint32_t shaded[] = {0x34808080, 400, 0, 0x808080, 400, 0x010B0000, 0x808080, 400, 0};
        const uint32_t untextured[] = {0x20808080, 400, 400, 400};
        uint32_t sprite[] = {0x65808080, 20 | ((20 + gouraud * 4) << 16), 0, 0x00010002};
        RECT destination = {20, 20 + gouraud * 4, 2, 1};
        uint16_t pixels[2] = {0};
        submit(&mode, 1);
        submit(gouraud ? shaded : flat, gouraud ? 9 : 7);
        submit(untextured, 4);
        submit(sprite, 4);
        DrawSync(0);

        if (StoreImage(&destination, (u_long*)pixels) < 0 || (pixels[0] & 0x7FFF) != 0x001F || (pixels[1] & 0x7FFF) != 0x001F)
        {
            fprintf(stderr, "Polygon TPAGE regression (%u): %04X %04X\n", gouraud, pixels[0], pixels[1]);
            return 1;
        }
    }

    puts("GPU polygon-to-sprite texture-page smoke passed (flat and Gouraud).");
    return 0;
}
