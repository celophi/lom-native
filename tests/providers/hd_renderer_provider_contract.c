#include "../../src/gpu/hd_renderer_provider.h"

#include <libgpu.h>
#include <psyz/gpu.h>
#include <psyz/video.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../support/lom_check.h"

#define LAYER_SIZE 2048
#define PAGE_SIZE 1024
#define CELL_SLOT 76
#define RED 0xFF0000
#define GREEN 0x00FF00
#define SCALE 2
#define COMMAND 0x1F
#define PAGES 0
#define PICTURES 1
#define PAGE_BITS 0x1000
#define PICTURE_BITS 0x1800
#define OPAQUE_AS_IT_IS 0x808080FFu

static void (*s_draw)(void);

/**
 * @brief Start the renderer, drawing to a 320 by 240 screen, cleared to black.
 */
static void start_the_renderer(void)
{
    DRAWENV draw;
    DISPENV display;
    RECT clear = {0, 0, 1024, 512};
    ResetGraph(0);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    SetDefDispEnv(&display, 0, 0, 320, 240);
    draw.dtd = 0;
    PutDrawEnv(&draw);
    PutDispEnv(&display);
    Psyz_VideoSetInternalResolution(SCALE);
    ClearImage(&clear, 0, 0, 0);
    DrawSync(0);
}

/**
 * @brief The GP0 command that makes the test's draws, since the renderer only takes HD draws while it runs a command,
 *        as the Remaster's are made.
 * @param words The command's words.
 * @param available How many there are.
 * @param userdata Unused.
 * @return The one word it takes.
 */
static int run_the_draws(const u_long* words, int available, void* userdata)
{
    (void)words;
    (void)available;
    (void)userdata;
    s_draw();
    return 1;
}

/**
 * @brief Have the renderer make some draws, as it runs a command, the way the Remaster's commands draw.
 * @param draw The draws.
 */
static void draw_while_running_a_command(void (*draw)(void))
{
    s_draw = draw;
    Psyz_GpuWriteGP0((unsigned)COMMAND << 24);
    Psyz_GpuExeque();
}

/**
 * @brief Read one pixel of what was drawn, from the picture the renderer made at twice the PS1's size, which is where
 *        the HD art goes.
 * @param x Where it is, on the PS1's screen.
 * @param y The same, down.
 * @return The pixel: red in the top byte, then green, then blue.
 */
static uint32_t drawn_at(int x, int y)
{
    int w = 0, h = 0;
    DrawSync(0);
    unsigned char* frame = Psyz_VideoAllocCapturedFrame(&w, &h);
    CHECK(frame && x * SCALE < w && y * SCALE < h);
    const unsigned char* pixel = frame + ((size_t)(y * SCALE) * (size_t)w + (size_t)(x * SCALE)) * 3;
    const uint32_t rgb = (uint32_t)pixel[0] << 16 | (uint32_t)pixel[1] << 8 | pixel[2];
    free(frame);
    return rgb;
}

/**
 * @brief Make a picture of one color, its bytes in the order the Remaster's pictures have them, which the renderer
 *        reads: blue, green, red, then alpha.
 * @param w Its width.
 * @param h Its height.
 * @param red Its red.
 * @param green Its green.
 * @param blue Its blue.
 * @return The pixels, which the caller frees.
 */
static unsigned char* picture_of(int w, int h, unsigned char red, unsigned char green, unsigned char blue)
{
    unsigned char* pixels = malloc((size_t)w * (size_t)h * 4);

    for (int i = 0; i < w * h; ++i)
    {
        pixels[i * 4] = blue;
        pixels[i * 4 + 1] = green;
        pixels[i * 4 + 2] = red;
        pixels[i * 4 + 3] = 255;
    }

    return pixels;
}

/**
 * @brief Queue the red picture over one square and the green one over the next, as quads of the pictures' texture.
 */
static void draw_the_two_pictures(void)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    renderer->quad(renderer->context, (const short[8]){10, 10, 26, 10, 10, 26, 26, 26}, (const unsigned short[8]){0, 0, 16, 0, 0, 16, 16, 16},
                   OPAQUE_AS_IT_IS, -1, PICTURE_BITS, 0);
    renderer->quad(renderer->context, (const short[8]){40, 10, 56, 10, 40, 26, 56, 26}, (const unsigned short[8]){16, 0, 32, 0, 16, 16, 32, 16},
                   OPAQUE_AS_IT_IS, -1, PICTURE_BITS, 0);
}

/**
 * @brief Queue the first page's first cell over a square, as a quad of the pages' texture.
 */
static void draw_the_first_cell(void)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    renderer->quad(renderer->context, (const short[8]){100, 100, 116, 100, 100, 116, 116, 116}, (const unsigned short[8]){0, 0, 64, 0, 0, 64, 64, 64},
                   OPAQUE_AS_IT_IS, -1, PAGE_BITS, 0);
}

/**
 * @brief Check the renderer keeps what's uploaded to the pictures' texture where it went, and that a quad with the
 *        pictures' bits shows the part of the layer its texture coordinates say, as the fake takes it to.
 * @note Given the pictures' texture with a red square uploaded at its top left and a green one beside it; when each is
 *       queued as a quad over its own square of the screen; then each square shows its picture's colour and the space
 *       between them nothing.
 */
static void draws_pictures_where_it_is_told(void)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    unsigned char* red = picture_of(16, 16, 255, 0, 0);
    unsigned char* green = picture_of(16, 16, 0, 255, 0);

    CHECK(renderer->texture_create && renderer->texture_upload && renderer->texture_mipmaps && renderer->texture_release && renderer->quad);
    CHECK(renderer->texture_create(renderer->context, PICTURES, 4, LAYER_SIZE, 5) == 0);
    CHECK(renderer->texture_upload(renderer->context, PICTURES, 0, 0, 0, 16, 16, red) == 0);
    CHECK(renderer->texture_upload(renderer->context, PICTURES, 0, 16, 0, 16, 16, green) == 0);
    renderer->texture_mipmaps(renderer->context, PICTURES);
    Psyz_GpuExeque();
    DrawSync(0);

    draw_while_running_a_command(draw_the_two_pictures);

    CHECK(drawn_at(18, 18) == RED);
    CHECK(drawn_at(48, 18) == GREEN);
    CHECK(drawn_at(32, 18) == 0);

    renderer->texture_release(renderer->context, PICTURES);
    free(red);
    free(green);
}

/**
 * @brief Check a quad with the pages' bits shows the cell of the page it names, as the Remaster's backgrounds take it
 *        to.
 * @note Given the pages' texture with a page whose first cell is green; when that cell is queued as a quad over a
 *       square of the screen; then the square shows green.
 */
static void draws_background_cells_where_it_is_told(void)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    unsigned char* page = picture_of(PAGE_SIZE, PAGE_SIZE, 0, 0, 0);

    for (int y = 0; y < CELL_SLOT; ++y)
    {
        for (int x = 0; x < CELL_SLOT; ++x)
        {
            page[((size_t)y * PAGE_SIZE + (size_t)x) * 4 + 1] = 255;
        }
    }

    CHECK(renderer->texture_create(renderer->context, PAGES, 1, PAGE_SIZE, 1) == 0);
    CHECK(renderer->texture_upload(renderer->context, PAGES, 0, 0, 0, PAGE_SIZE, PAGE_SIZE, page) == 0);
    Psyz_GpuExeque();
    DrawSync(0);

    draw_while_running_a_command(draw_the_first_cell);

    CHECK(drawn_at(108, 108) == GREEN);

    renderer->texture_release(renderer->context, PAGES);
    free(page);
}

int main(void)
{
    start_the_renderer();
    Psyz_GpuRegisterCommandHandler(COMMAND, run_the_draws, NULL);
    draws_pictures_where_it_is_told();
    draws_background_cells_where_it_is_told();
    puts("hd-renderer-provider: keeps HD textures and draws quads of them where it's told");
    return 0;
}
