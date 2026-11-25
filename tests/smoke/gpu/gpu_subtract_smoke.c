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
    RECT source = {640, 0, 4, 1};
    uint16_t texels[4] = {0x7FFF, 0xA108, 0x0000, 0x8000};
    ResetGraph(0);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    SetDefDispEnv(&display, 0, 0, 320, 240);
    draw.dtd = 0;
    PutDrawEnv(&draw);
    PutDispEnv(&display);
    ClearImage(&clear, 128, 128, 128);
    LoadImage(&source, (u_long*)texels);
    DrawSync(0);

    for (unsigned indexed = 0; indexed < 2; ++indexed)
    {
        if (indexed)
        {
            RECT palette = {768, 10, 256, 1};
            uint16_t colors[256] = {0};

            for (unsigned i = 0; i < 4; ++i)
            {
                colors[i] = texels[i];
            }

            LoadImage(&palette, (u_long*)colors);
            uint16_t indices[4] = {0x0100, 0x0302, 0, 0};
            LoadImage(&source, (u_long*)indices);
        }

        unsigned page = (indexed ? 0x80 : 0x100) | 0x40 | 10;
        uint32_t mode = 0xE1000000 | page;
        unsigned y = 20 + indexed * 4;
        uint32_t packets[] = {
            mode,       0x67808080, 20 | (y << 16), (indexed ? 0x02B00000 : 0), 0x00010004, 0x67808080, 20 | (y << 16), (indexed ? 0x02B00000 : 0) | 1,
            0x00010001, 0x65808080, 24 | (y << 16), (indexed ? 0x02B00000 : 0), 0x00010004,
        };
        submit(packets, sizeof(packets) / sizeof(packets[0]));
        RECT destination = {20, y, 8, 1};
        uint16_t pixels[8] = {0};
        const uint16_t expected[8] = {0x5EF7, 0x2108, 0x4210, 0x4210, 0x7FFF, 0x2108, 0x4210, 0};

        if (StoreImage(&destination, (u_long*)pixels) < 0)
        {
            return 1;
        }

        for (unsigned i = 0; i < 8; ++i)
        {
            if ((pixels[i] & 0x7FFF) != expected[i])
            {
                fprintf(stderr, "Subtractive texture regression (%s pixel %u): %04X expected %04X\n", indexed ? "8-bit" : "16-bit", i, pixels[i], expected[i]);
                return 1;
            }
        }
    }

    puts("GPU mixed opaque/subtractive texels and overlap order passed (16-bit and 8-bit).");
    return 0;
}
