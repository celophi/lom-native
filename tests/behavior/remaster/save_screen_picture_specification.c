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
#define DATA REMASTER_FOLDER "/windata/"
#define SPRITE_WORDS 4
#define PARCHMENT 0x907050FFu
#define DARK_ROW 0x202020FFu
#define CHOSEN_ROW 0xF080A0FFu
#define HERO 0xE0B090FFu
#define HEROINE 0xF0C0A0FFu
#define ARROW_UP 0x101080FFu
#define ARROW_DOWN 0x1010A0FFu
#define WINDOW 0x403020FFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static RemasterPicture s_parchment[] = {{"sl_losd_bg", 1920, 1080, PARCHMENT, 0, NULL, 0, NULL}};
static RemasterPicture s_dark_row[] = {{"load_dark", 8, 4, DARK_ROW, 0, NULL, 0, NULL}};
static RemasterPicture s_chosen_row[] = {{"load_pink", 8, 4, CHOSEN_ROW, 0, NULL, 0, NULL}};
static RemasterPicture s_faces[] = {{"hero", 120, 120, HERO, 0, NULL, 0, NULL}, {"heroine", 120, 120, HEROINE, 0, NULL, 0, NULL}};
static RemasterPicture s_arrow_up[] = {{"sl_load_arrow_up", 40, 30, ARROW_UP, 0, NULL, 0, NULL}};
static RemasterPicture s_arrow_down[] = {{"sl_load_arrow_down", 40, 30, ARROW_DOWN, 0, NULL, 0, NULL}};
static RemasterPicture s_window[] = {{"sl_text_winddow", 600, 200, WINDOW, 0, NULL, 0, NULL}};
static int s_hd_screens = 1;
static int s_widescreen = 1;

/**
 * @brief Put in a fake of each provider the save screens use, with the happy path's defaults: the Remaster in Steam's
 *        library with the save screens' pictures and its font, and a renderer that can draw them.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Give the fake one of the Remaster's picture files.
 * @param path The file.
 * @param pictures Its pictures.
 * @param count How many.
 */
static void the_remaster_has(const char* path, const RemasterPicture* pictures, int count)
{
    size_t size = 0;
    unsigned char* file = remaster_psb_picture_file(pictures, count, &size);
    remaster_files_provider_fake_add(&s_files_fake, path, file, size);
}

/**
 * @brief Start the game with the launcher's settings and the player's Widescreen setting, then open a save screen,
 *        which is when it decides whether it's laid out as the Remaster's.
 */
static void open_a_save_screen(void)
{
    char hd_screens[8];
    size_t size = 0;
    the_remaster_has(DATA "system/image/sl_losd_bg.psb.m", s_parchment, 1);
    the_remaster_has(DATA "system/image/load_dark.psb.m", s_dark_row, 1);
    the_remaster_has(DATA "system/image/load_pink.psb.m", s_chosen_row, 1);
    the_remaster_has(DATA "icon_pc.psb.m", s_faces, 2);
    the_remaster_has(DATA "system/image/sl_load_arrow_up.psb.m", s_arrow_up, 1);
    the_remaster_has(DATA "system/image/sl_load_arrow_down.psb.m", s_arrow_down, 1);
    the_remaster_has(DATA "system/image/sl_text_winddow.psb.m", s_window, 1);
    unsigned char* font = remaster_psb_font_file("D\0a\0y\0 \0003\0", 45, &size);
    remaster_files_provider_fake_add(&s_files_fake, DATA "system/font/font_us.psb.m", font, size);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    lom_native_remaster_initialize();
    lom_native_widescreen_set_enabled(s_widescreen);
    lom_draw_cards_begin();
}

static int s_picture = LOM_CARDS_ROW_SELECTED;
static SPRT s_sprite;

/**
 * @brief Open a save screen, then have it build a sprite for one of its pictures, hand it to the port with where the
 *        picture goes, and draw it, as the save screens do each frame.
 */
static void when_a_save_screen_draws_a_picture(void)
{
    open_a_save_screen();
    setSprt(&s_sprite);
    setRGB0(&s_sprite, 0x80, 0x80, 0x80);
    setXY0(&s_sprite, 30, 50);
    setWH(&s_sprite, 16, 16);
    lom_draw_cards_picture(&s_sprite.r0, s_picture, 24, 48, 272, 22);
    gpu_draw(&s_sprite.r0, SPRITE_WORDS, sizeof(SPRT), 1);
}

/**
 * @brief Given the defaults, the chosen save's row; when a save screen draws it; then the Remaster's pink band is
 *        stretched over the row.
 */
SPECIFICATION(draws_the_chosen_row, "Save screen picture should stretch the Remaster's pink band over the chosen row")
{
    when_a_save_screen_draws_a_picture();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == CHOSEN_ROW);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){24, 48, 296, 48, 24, 70, 296, 70}, sizeof(int[8])) == 0);
}

/**
 * @brief Given a row that isn't chosen; when a save screen draws it; then the Remaster's dark band is drawn.
 */
SPECIFICATION(draws_the_dark_row, "Save screen picture should draw the Remaster's dark band for a row that isn't chosen")
{
    s_picture = LOM_CARDS_ROW_DARK;

    when_a_save_screen_draws_a_picture();

    CHECK(s_renderer_fake.quads[0].colour == DARK_ROW);
}

/**
 * @brief Given a save of the heroine's; when a save screen draws its face; then the Remaster's heroine is drawn.
 */
SPECIFICATION(draws_the_heroines_face, "Save screen picture should draw the Remaster's heroine for her save")
{
    s_picture = LOM_CARDS_FACE_HEROINE;

    when_a_save_screen_draws_a_picture();

    CHECK(s_renderer_fake.quads[0].colour == HEROINE);
}

/**
 * @brief Given more saves below; when a save screen draws its arrow; then the Remaster's down arrow is drawn.
 */
SPECIFICATION(draws_the_down_arrow, "Save screen picture should draw the Remaster's down arrow when there are more saves below")
{
    s_picture = LOM_CARDS_ARROW_DOWN;

    when_a_save_screen_draws_a_picture();

    CHECK(s_renderer_fake.quads[0].colour == ARROW_DOWN);
}

/**
 * @brief Given Widescreen is off, so the save screen is the PS1's; when it draws the chosen row; then the PS1's sprite
 *        is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_pictures, "Save screen picture should leave the PS1's pictures when the screen is the PS1's")
{
    s_widescreen = 0;

    when_a_save_screen_draws_a_picture();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_sprite.code == 0x64);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
