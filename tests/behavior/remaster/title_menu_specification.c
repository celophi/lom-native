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
#define QUAD_WORDS 9
#define NEW_GAME 0
#define CONTINUE 1
#define NEW_GAME_WORDS 0xF0E0D0FFu
#define CONTINUE_WORDS 0xD0E0F0FFu
#define COPYRIGHT 0xA0A0A0FFu
#define CURSOR_FRAME_1 0x101010FFu
#define CURSOR_FRAME_2 0x202020FFu
#define CURSOR_FRAME_3 0x303030FFu
#define CURSOR_FRAME_4 0x404040FFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static const uint32_t s_cursor_frames[] = {CURSOR_FRAME_1, CURSOR_FRAME_2, CURSOR_FRAME_3, CURSOR_FRAME_4};
static RemasterPicture s_title_pictures[] = {
    {"background", 1920, 1080, 0x305070FFu, 0, NULL},
    {"logo-en", 1920, 1080, 0xE0C040FFu, 0, NULL},
    {"new_game_asia", 270, 45, NEW_GAME_WORDS, 0, NULL},
    {"load_game_asia", 270, 45, CONTINUE_WORDS, 0, NULL},
    {"copyright_all", 810, 54, COPYRIGHT, 0, NULL},
    {"cursor", 64, 16, 0, 4, s_cursor_frames},
};
static int s_title_picture_count = 6;
static int s_hd_screens = 1;
static int s_part = LOM_TITLE_MENU_ITEM;
static int s_item = NEW_GAME;
static int s_chosen = 1;
static int s_x = 0x88;
static int s_y = 0xA0;
static int s_cursor_u = 0;
static POLY_FT4 s_quad;

/**
 * @brief Put in a fake of each provider the title's menu uses, with the happy path's defaults: the Remaster in
 *        Steam's library with its title art, and a renderer that can draw it.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Start the game with the launcher's settings, then have the title build one part of its menu as a quad of the
 *        PS1's menu texture, hand it to the port and draw it, as TITLE does each frame.
 */
static void when_the_title_draws_part_of_its_menu(void)
{
    size_t size = 0;
    char hd_screens[8];
    unsigned char* title = remaster_psb_picture_file(s_title_pictures, s_title_picture_count, &size);
    remaster_files_provider_fake_add(&s_files_fake, TITLE_FILE, title, size);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    lom_native_remaster_initialize();

    SetPolyFT4(&s_quad);
    setRGB0(&s_quad, 0x80, 0x80, 0x80);
    setXYWH(&s_quad, s_x, s_y, 0x80, 0x10);
    setUVWH(&s_quad, s_cursor_u, 0x10, 0x80, 0x10);
    lom_draw_title_menu(&s_quad.r0, s_part, s_item, s_chosen);
    gpu_draw(&s_quad.r0, QUAD_WORDS, sizeof(POLY_FT4), 1);
}

/**
 * @brief Given the defaults, New Game chosen; when the title draws New Game; then the Remaster's New Game is drawn as
 *        it is, at the PS1's size (two ninths of the Remaster's), from the left of the PS1's word and on its middle.
 */
SPECIFICATION(draws_new_game_chosen, "Title menu should draw the Remaster's New Game as it is when it's chosen")
{
    when_the_title_draws_part_of_its_menu();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == NEW_GAME_WORDS);
    CHECK(s_renderer_fake.quads[0].rgb == 0x808080);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){137, 164, 197, 164, 137, 174, 197, 174}, sizeof(int[8])) == 0);
}

/**
 * @brief Given Continue isn't the one chosen; when the title draws Continue; then the Remaster's Continue is drawn
 *        dimmed, as the PS1 dims the words not chosen.
 */
SPECIFICATION(dims_continue_when_not_chosen, "Title menu should dim the Remaster's Continue when it isn't chosen")
{
    s_item = CONTINUE;
    s_chosen = 0;
    s_y = 0xAC;

    when_the_title_draws_part_of_its_menu();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == CONTINUE_WORDS);
    CHECK(s_renderer_fake.quads[0].rgb == 0x505050);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){137, 176, 197, 176, 137, 186, 197, 186}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the PS1's cursor is on its third frame; when the title draws its cursor; then the Remaster's cursor's
 *        third frame is drawn over the PS1's, at the same size.
 */
SPECIFICATION(draws_the_cursor_frame_shown, "Title menu should draw the frame of the Remaster's cursor that the PS1's cursor shows")
{
    s_part = LOM_TITLE_MENU_CURSOR;
    s_x = 0x78;
    s_y = 0x9D;
    s_cursor_u = 32;

    when_the_title_draws_part_of_its_menu();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == CURSOR_FRAME_3);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){120, 157, 136, 157, 120, 173, 136, 173}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the defaults; when the title draws its copyright line; then the Remaster's is drawn at the PS1's size,
 *        centred on the middle of the PS1's.
 */
SPECIFICATION(centres_the_copyright, "Title menu should centre the Remaster's copyright line on the PS1's")
{
    s_part = LOM_TITLE_MENU_COPYRIGHT;
    s_x = 0x64;
    s_y = 0xC8;

    when_the_title_draws_part_of_its_menu();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == COPYRIGHT);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){62, 201, 242, 201, 62, 213, 242, 213}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the title draws New Game; then nothing of the
 *        Remaster's is drawn and the PS1's quad is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_menu_when_hd_screens_are_off, "Title menu should leave the PS1's menu when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_title_draws_part_of_its_menu();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_quad.code == 0x2C && s_quad.x0 == 0x88 && s_quad.y0 == 0xA0);
}

/**
 * @brief Given the Remaster's title has no logo for the text's language, so its title art isn't drawn; when the title
 *        draws New Game; then the PS1's menu is left too, so the screen is all the PS1's.
 */
SPECIFICATION(leaves_the_ps1_menu_without_the_backdrop, "Title menu should leave the PS1's menu when the Remaster's backdrop can't be drawn")
{
    s_title_pictures[1].label = "not-a-logo";

    when_the_title_draws_part_of_its_menu();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_quad.code == 0x2C);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
