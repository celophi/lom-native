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

static SPRT s_sprites[2];

/**
 * @brief Open a save screen, then have it build its background as two sprites of the PS1's, hand them to the port and
 *        draw them, as the save screens do each frame.
 */
static void when_a_save_screen_draws_its_background(void)
{
    open_a_save_screen();

    for (int i = 0; i < 2; ++i)
    {
        setSprt(&s_sprites[i]);
        setRGB0(&s_sprites[i], 0x80, 0x80, 0x80);
        setXY0(&s_sprites[i], i * 160, 0);
        setWH(&s_sprites[i], 160, 224);
    }

    lom_draw_cards_background(&s_sprites[0].r0, sizeof(SPRT), 2);
    gpu_draw(&s_sprites[0].r0, SPRITE_WORDS, sizeof(SPRT), 2);
}

/**
 * @brief Given the defaults; when a save screen draws its background; then the Remaster's burnt parchment is drawn once
 *        over the whole screen, and the PS1's sprites draw nothing.
 */
SPECIFICATION(draws_the_parchment_once, "Save screen background should draw the Remaster's parchment once over the whole screen")
{
    when_a_save_screen_draws_its_background();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == PARCHMENT);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){0, 0, 320, 0, 0, 224, 320, 224}, sizeof(int[8])) == 0);
    CHECK(s_sprites[1].code != 0x64);
}

/**
 * @brief Given Widescreen is off, so the save screen is the PS1's; when it draws its background; then the PS1's sprites
 *        are left as the game built them.
 */
SPECIFICATION(leaves_the_ps1_background, "Save screen background should leave the PS1's background when the screen is the PS1's")
{
    s_widescreen = 0;

    when_a_save_screen_draws_its_background();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprites[0].code == 0x64);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
