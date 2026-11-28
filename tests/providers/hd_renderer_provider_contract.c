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
 * @brief Draw the red picture over one square and the green one over the next.
 */
static void draw_the_two_pictures(void)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    renderer->images_draw(renderer->context, (const int[8]){10, 10, 26, 10, 10, 26, 26, 26}, (const int[8]){0, 0, 16, 0, 0, 16, 16, 16}, 0, 0x808080, 0,
                          -1);
    renderer->images_draw(renderer->context, (const int[8]){40, 10, 56, 10, 40, 26, 56, 26}, (const int[8]){16, 0, 32, 0, 16, 16, 32, 16}, 0, 0x808080, 0,
                          -1);
}

/**
 * @brief Draw the first page's first cell over a square.
 */
static void draw_the_first_cell(void)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    renderer->textures_draw(renderer->context, (const int[8]){100, 100, 116, 100, 100, 116, 116, 116}, 0, 0x00, 0x808080, 0, -1);
}

/**
 * @brief Check the renderer's picture calls keep what's uploaded where it went and draw it where they're told, as the
 *        fake does, and refuse what doesn't fit.
 * @note Given a layer with a red picture uploaded at its top left and a green one beside it; when each is drawn over
 *       its own square of the screen; then each square shows its picture's color, and an upload past the layer's
 *       edge or to a layer that isn't there is refused.
 */
static void draws_pictures_where_it_is_told(void)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    unsigned char* red = picture_of(16, 16, 255, 0, 0);
    unsigned char* green = picture_of(16, 16, 0, 255, 0);

    CHECK(renderer->images_set && renderer->images_upload && renderer->images_draw && renderer->images_release);
    CHECK(renderer->images_set(renderer->context, 4, LAYER_SIZE) == 0);
    CHECK(renderer->images_upload(renderer->context, 0, 0, 0, 16, 16, red) == 0);
    CHECK(renderer->images_upload(renderer->context, 0, 16, 0, 16, 16, green) == 0);
    CHECK(renderer->images_upload(renderer->context, 0, LAYER_SIZE - 8, 0, 16, 16, red) != 0);
    CHECK(renderer->images_upload(renderer->context, 4, 0, 0, 16, 16, red) != 0);

    Psyz_GpuExeque();
    DrawSync(0);
    draw_while_running_a_command(draw_the_two_pictures);

    CHECK(drawn_at(18, 18) == RED);
    CHECK(drawn_at(48, 18) == GREEN);
    CHECK(drawn_at(32, 18) == 0);

    renderer->images_release(renderer->context);
    free(red);
    free(green);
}

/**
 * @brief Check the renderer's background calls draw a cell of an uploaded page where they're told, as the fake does.
 * @note Given a page whose first cell is green; when that cell is drawn over a square of the screen; then the square
 *       shows green, and an upload to a page that isn't there is refused.
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

    CHECK(renderer->textures_set && renderer->textures_upload && renderer->textures_draw && renderer->textures_release);
    CHECK(renderer->textures_set(renderer->context, 1, PAGE_SIZE) == 0);
    CHECK(renderer->textures_upload(renderer->context, 0, page) == 0);
    CHECK(renderer->textures_upload(renderer->context, 1, page) != 0);

    Psyz_GpuExeque();
    DrawSync(0);
    draw_while_running_a_command(draw_the_first_cell);

    CHECK(drawn_at(108, 108) == GREEN);

    renderer->textures_release(renderer->context);
    free(page);
}

/**
 * @brief Check the renderer has the movie calls the fake stands in for, and starts an HD movie of the Remaster's size.
 * @note Given the renderer; when an HD movie starts, shows a frame and ends; then starting it succeeds.
 */
static void starts_an_hd_movie(void)
{
    const LomHdRendererProvider* renderer = lom_hd_renderer_provider();
    unsigned char* frame = picture_of(1280, 720, 0, 0, 255);

    CHECK(renderer->movie_begin && renderer->movie_frame && renderer->movie_end);
    CHECK(renderer->movie_begin(renderer->context, 1280, 720) == 0);
    renderer->movie_frame(renderer->context, frame);
    renderer->movie_end(renderer->context);
    free(frame);
}

int main(void)
{
    start_the_renderer();
    Psyz_GpuRegisterCommandHandler(COMMAND, run_the_draws, NULL);
    draws_pictures_where_it_is_told();
    draws_background_cells_where_it_is_told();
    starts_an_hd_movie();
    puts("hd-renderer-provider: draws HD pictures and backgrounds where it's told, and starts HD movies");
    return 0;
}
