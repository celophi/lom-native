#include "lom_native_draw.h"
#include "lom_native_env.h"
#include "lom_native_remaster.h"
#include "lom_native_widescreen.h"

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
#define NAMING_FILE REMASTER_FOLDER "/windata/name_input.psb.m"
#define SPRITE_WORDS 4
#define ENGLISH_BACKGROUND 0x806040FFu
#define GERMAN_BACKGROUND 0x408060FFu
#define TYPEWRITER 0x303030FFu
#define PAPER 0xF0F0E0FFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static RemasterPicture s_naming_pictures[] = {
    {"bg_en", 1920, 1080, ENGLISH_BACKGROUND, 0, NULL, 0, NULL},
    {"bg_de", 1920, 1080, GERMAN_BACKGROUND, 0, NULL, 0, NULL},
    {"typewriter_paper", 190, 33, PAPER, 0, NULL, 0, NULL},
    {"typewriter", 64, 56, TYPEWRITER, 0, NULL, 0, NULL},
    {"typewriter_anim_paper1", 20, 34, 0x101010FFu, 0, NULL, 0, NULL},
    {"typewriter_anim_paper2", 20, 34, 0x202020FFu, 0, NULL, 0, NULL},
    {"typewriter_anim_paper3", 20, 34, 0x303030FFu, 0, NULL, 0, NULL},
    {"typewriter_anim_paper4", 20, 34, 0x404040FFu, 0, NULL, 0, NULL},
    {"typewriter_anim_hand1", 20, 23, 0x505050FFu, 0, NULL, 0, NULL},
    {"typewriter_anim_hand2", 20, 23, 0x606060FFu, 0, NULL, 0, NULL},
};
static int s_hd_screens = 1;
static int s_widescreen = 1;
static int s_text = LOM_REMASTER_TEXT_ENGLISH;

/**
 * @brief Put in a fake of each provider the naming screen uses, with the happy path's defaults: the Remaster in
 *        Steam's library with the naming screen's art, and a renderer that can draw it.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Start the game with the launcher's settings and the player's Widescreen setting, then open the naming screen,
 *        which is when it decides whether it's laid out as the Remaster's.
 */
static void open_the_naming_screen(void)
{
    size_t size = 0;
    char hd_screens[8], text[8];
    unsigned char* art = remaster_psb_picture_file(s_naming_pictures, 10, &size);
    remaster_files_provider_fake_add(&s_files_fake, NAMING_FILE, art, size);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    snprintf(text, sizeof(text), "%d", s_text);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    setenv(LOM_ENV_REMASTER_TEXT, text, 1);
    lom_native_remaster_initialize();
    lom_native_widescreen_set_enabled(s_widescreen);
    lom_remaster_gname_begin();
}
#define GLYPH_PAPER 2
#define GLYPH_TYPEWRITER 3
#define GLYPH_FIRST_TAB 26
#define GLYPH_OK 30
#define GLYPH_DELETE 31

static int s_glyph = GLYPH_TYPEWRITER;
static int s_glyph_rect[4] = {120, 150, 64, 56};
static SPRT s_sprites[2];

/**
 * @brief Open the naming screen, then have it build one of its glyphs as two sprites of the PS1's sheet, hand them to
 *        the port and draw them, as GNAME does each frame.
 */
static void when_the_naming_screen_draws_a_glyph(void)
{
    open_the_naming_screen();

    for (int i = 0; i < 2; ++i)
    {
        setSprt(&s_sprites[i]);
        setRGB0(&s_sprites[i], 0x80, 0x80, 0x80);
        setXY0(&s_sprites[i], s_glyph_rect[0] + i * 8, s_glyph_rect[1]);
        setWH(&s_sprites[i], s_glyph_rect[2], s_glyph_rect[3]);
        setUV0(&s_sprites[i], 16, 32);
    }

    lom_draw_gname_glyph(&s_sprites[0].r0, 2, sizeof(SPRT), s_glyph);
    gpu_draw(&s_sprites[0].r0, SPRITE_WORDS, sizeof(SPRT), 2);
}

/**
 * @brief Given the defaults, the typewriter; when the naming screen draws it; then the Remaster's typewriter is drawn
 *        where the PS1's sits in its box.
 */
SPECIFICATION(draws_the_typewriter, "Naming screen glyph should draw the Remaster's typewriter where the PS1's sits")
{
    when_the_naming_screen_draws_a_glyph();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == TYPEWRITER);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){120, 150, 184, 150, 120, 206, 184, 206}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the paper the name is typed on; when the naming screen draws it; then the Remaster's paper is drawn
 *        where the PS1's sits in its box, a pixel in.
 */
SPECIFICATION(draws_the_paper, "Naming screen glyph should draw the Remaster's paper where the PS1's sits")
{
    s_glyph = GLYPH_PAPER;
    memcpy(s_glyph_rect, (int[4]){60, 30, 192, 33}, sizeof(s_glyph_rect));

    when_the_naming_screen_draws_a_glyph();

    CHECK(s_renderer_fake.pictures[0].colour == PAPER);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){61, 30, 251, 30, 61, 63, 251, 63}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the OK button; when the naming screen draws it; then nothing is drawn for it, since the Remaster's
 *        background has its buttons painted in.
 */
SPECIFICATION(draws_no_buttons, "Naming screen glyph should draw nothing for a button the Remaster's background has painted in")
{
    s_glyph = GLYPH_OK;

    when_the_naming_screen_draws_a_glyph();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprites[0].code != 0x64 && s_sprites[1].code != 0x64);
}

/**
 * @brief Given the delete button, whose arrow the Remaster's background doesn't have; when the naming screen draws it;
 *        then the PS1's arrow alone is drawn, moved onto the Remaster's delete button in the column, and the rest of
 *        the PS1's button draws nothing.
 */
SPECIFICATION(moves_the_delete_arrow, "Naming screen glyph should move the PS1's delete arrow onto the Remaster's delete button")
{
    s_glyph = GLYPH_DELETE;

    when_the_naming_screen_draws_a_glyph();

    CHECK(s_sprites[0].code == 0x64);
    CHECK(s_sprites[0].x0 == 54 && s_sprites[0].y0 == 116 && s_sprites[0].w == 21 && s_sprites[0].h == 9);
    CHECK(s_sprites[0].u0 == 25 && s_sprites[0].v0 == 36);
    CHECK(s_sprites[1].code != 0x64);
}

/**
 * @brief Given a tab; when the naming screen draws it; then the PS1's tab is drawn 60 pixels further left, beside the
 *        Remaster's wider paper.
 */
SPECIFICATION(moves_the_tabs_left, "Naming screen glyph should move the PS1's tabs left of the Remaster's paper")
{
    s_glyph = GLYPH_FIRST_TAB;

    when_the_naming_screen_draws_a_glyph();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprites[0].x0 == 60 && s_sprites[1].x0 == 68);
}

/**
 * @brief Given Widescreen is off, so the naming screen keeps the PS1's layout; when it draws its typewriter; then the
 *        PS1's sprites are left as the game built them.
 */
SPECIFICATION(leaves_the_ps1_glyphs_in_the_ps1_layout, "Naming screen glyph should leave the PS1's glyphs in the PS1's layout")
{
    s_widescreen = 0;

    when_the_naming_screen_draws_a_glyph();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprites[0].code == 0x64 && s_sprites[0].x0 == 120);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
