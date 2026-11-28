#include "lom_native_draw.h"
#include "lom_native_env.h"
#include "lom_native_remaster.h"

#include <libgpu.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../support/behavior/specification.h"
#include "../../support/gpu/gpu_draw.h"
#include "../../support/gpu/hd_renderer_provider_fake.h"
#include "../../support/lom_check.h"
#include "../../support/platform/lom_test_posix.h"
#include "../../support/platform/remaster_files_provider_fake.h"
#include "../../support/remaster/remaster_psb_builder.h"

#define REMASTER_FOLDER "/steam/steamapps/common/Legend of Mana"
#define TITLE_FILE REMASTER_FOLDER "/windata/system/image/title/title.psb.m"
#define STRIPS 5
#define QUAD_WORDS 9
#define STRIP_WIDTH 64
#define SCREEN_HEIGHT 224
#define BACKGROUND 0x305070FFu
#define ENGLISH_LOGO 0xE0C040FFu
#define JAPANESE_LOGO 0xC02020FFu
#define KOREAN_LOGO 0x2040C0FFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static RemasterPicture s_title_pictures[] = {
    {"background", 1920, 1080, BACKGROUND, 0, NULL},
    {"logo-en", 1920, 1080, ENGLISH_LOGO, 0, NULL},
    {"logo-ja", 1920, 1080, JAPANESE_LOGO, 0, NULL},
    {"logo-ko", 1920, 1080, KOREAN_LOGO, 0, NULL},
};
static int s_title_picture_count = 4;
static const char* s_remaster_folder = REMASTER_FOLDER;
static int s_text = LOM_REMASTER_TEXT_ENGLISH;
static int s_hd_screens = 1;
static POLY_FT4 s_strips[STRIPS];

/**
 * @brief Put in a fake of each provider the title's backdrop uses, with the happy path's defaults: the Remaster in
 *        Steam's library with its title art, and a renderer that can draw it.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Start the game with the launcher's settings, then have the title build its backdrop as five strips of the
 *        PS1's picture, hand them to the port and draw them, as TITLE does each frame.
 */
static void when_the_title_draws_its_backdrop(void)
{
    size_t size = 0;
    unsigned char* title = remaster_psb_picture_file(s_title_pictures, s_title_picture_count, &size);
    remaster_files_provider_fake_add(&s_files_fake, TITLE_FILE, title, size);
    char text[8], hd_screens[8];
    snprintf(text, sizeof(text), "%d", s_text);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    setenv(LOM_ENV_REMASTER_ROOT, s_remaster_folder, 1);
    setenv(LOM_ENV_REMASTER_TEXT, text, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    lom_native_remaster_initialize();

    for (int i = 0; i < STRIPS; ++i)
    {
        POLY_FT4* strip = &s_strips[i];
        SetPolyFT4(strip);
        setRGB0(strip, 0x80, 0x80, 0x80);
        setXY4(strip, i * STRIP_WIDTH, 0, (i + 1) * STRIP_WIDTH, 0, i * STRIP_WIDTH, SCREEN_HEIGHT, (i + 1) * STRIP_WIDTH, SCREEN_HEIGHT);
        setUV4(strip, 0, 8, STRIP_WIDTH, 8, 0, 8 + SCREEN_HEIGHT, STRIP_WIDTH, 8 + SCREEN_HEIGHT);
    }

    lom_draw_title_backdrop(&s_strips[0].r0, sizeof(POLY_FT4), STRIPS);

    gpu_draw(&s_strips[0].r0, QUAD_WORDS, sizeof(POLY_FT4), STRIPS);
}

/**
 * @brief Given nothing more than the defaults; when the title draws its backdrop; then each strip draws the Remaster's
 *        background over its part of the screen, then the English logo over the same part, and the PS1's strips draw
 *        nothing of their own.
 */
SPECIFICATION(draws_the_background_and_the_english_logo, "Title backdrop should draw the Remaster's background with its English logo over it")
{
    when_the_title_draws_its_backdrop();

    CHECK(s_renderer_fake.quad_count == 2 * STRIPS);
    CHECK(s_renderer_fake.quads[0].colour == BACKGROUND);
    CHECK(s_renderer_fake.quads[1].colour == ENGLISH_LOGO);
    CHECK(s_renderer_fake.quads[8].colour == BACKGROUND);
    CHECK(s_renderer_fake.quads[9].colour == ENGLISH_LOGO);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){0, 0, 64, 0, 0, 224, 64, 224}, sizeof(int[8])) == 0);
    CHECK(memcmp(s_renderer_fake.quads[1].xy, (int[8]){0, 0, 64, 0, 0, 224, 64, 224}, sizeof(int[8])) == 0);
    CHECK(memcmp(s_renderer_fake.quads[8].xy, (int[8]){256, 0, 320, 0, 256, 224, 320, 224}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the Remaster's text is in Japanese; when the title draws its backdrop; then the logo drawn over the
 *        background is the Japanese one.
 */
SPECIFICATION(draws_the_japanese_logo, "Title backdrop should draw the Japanese logo when the Remaster's text is in Japanese")
{
    s_text = LOM_REMASTER_TEXT_JAPANESE;

    when_the_title_draws_its_backdrop();

    CHECK(s_renderer_fake.quad_count == 2 * STRIPS);
    CHECK(s_renderer_fake.quads[0].colour == BACKGROUND);
    CHECK(s_renderer_fake.quads[1].colour == JAPANESE_LOGO);
}

/**
 * @brief Given the Remaster's text is in Korean; when the title draws its backdrop; then the logo drawn is the Korean
 *        one.
 */
SPECIFICATION(draws_the_korean_logo, "Title backdrop should draw the Korean logo when the Remaster's text is in Korean")
{
    s_text = LOM_REMASTER_TEXT_KOREAN;

    when_the_title_draws_its_backdrop();

    CHECK(s_renderer_fake.quads[1].colour == KOREAN_LOGO);
}

/**
 * @brief Given the Remaster's text is in French, which the Remaster has no logo of its own for; when the title draws
 *        its backdrop; then the logo drawn is the English one.
 */
SPECIFICATION(draws_the_english_logo_for_french, "Title backdrop should draw the English logo when the Remaster's text is in French")
{
    s_text = LOM_REMASTER_TEXT_FRENCH;

    when_the_title_draws_its_backdrop();

    CHECK(s_renderer_fake.quads[1].colour == ENGLISH_LOGO);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the title draws its backdrop; then nothing of the
 *        Remaster's is drawn and the PS1's strips are left as the game built them.
 */
SPECIFICATION(leaves_the_ps1_backdrop_when_hd_screens_are_off, "Title backdrop should leave the PS1's backdrop when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_title_draws_its_backdrop();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_strips[0].code == 0x2C);
    CHECK(s_strips[4].x0 == 256 && s_strips[4].x3 == 320 && s_strips[4].y3 == 224);
}

/**
 * @brief Given the Remaster's text is in Japanese but its title art has no Japanese logo; when the title draws its
 *        backdrop; then the PS1's backdrop is left whole rather than drawing the Remaster's background without a logo.
 */
SPECIFICATION(leaves_the_ps1_backdrop_without_the_logo, "Title backdrop should leave the PS1's backdrop when the Remaster has no logo for the language")
{
    s_text = LOM_REMASTER_TEXT_JAPANESE;
    s_title_pictures[2].label = "not-a-logo";

    when_the_title_draws_its_backdrop();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_strips[0].code == 0x2C);
}

/**
 * @brief Given a renderer that can't draw HD art; when the title draws its backdrop; then the PS1's
 *        backdrop is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_backdrop_without_hd_art, "Title backdrop should leave the PS1's backdrop when the renderer can't draw HD art")
{
    hd_renderer_provider_fake_without_hd(&s_renderer_fake);

    when_the_title_draws_its_backdrop();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_strips[0].code == 0x2C);
}

/**
 * @brief Given the Remaster isn't where the launcher said it is; when the title draws its backdrop; then the PS1's
 *        backdrop is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_backdrop_without_the_remaster, "Title backdrop should leave the PS1's backdrop when the Remaster isn't installed")
{
    s_remaster_folder = "/nowhere";

    when_the_title_draws_its_backdrop();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_strips[0].code == 0x2C);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
