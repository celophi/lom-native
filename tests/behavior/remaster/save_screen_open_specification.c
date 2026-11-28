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

/**
 * @brief Open a save screen, as CLOAD and CARDA do.
 */
static void when_a_save_screen_opens(void)
{
    open_a_save_screen();
}

/**
 * @brief Given the defaults, Widescreen and HD screens on; when a save screen opens; then it's laid out as the
 *        Remaster's.
 */
SPECIFICATION(uses_the_remasters_layout, "Save screen should be laid out as the Remaster's when Widescreen and HD screens are on")
{
    when_a_save_screen_opens();

    CHECK(lom_draw_cards_on());
}

/**
 * @brief Given the player has Widescreen off, which the Remaster's wide save screens need; when a save screen opens;
 *        then it's the PS1's.
 */
SPECIFICATION(keeps_the_ps1_layout_without_widescreen, "Save screen should be the PS1's when Widescreen is off")
{
    s_widescreen = 0;

    when_a_save_screen_opens();

    CHECK(!lom_draw_cards_on());
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when a save screen opens; then it's the PS1's.
 */
SPECIFICATION(keeps_the_ps1_layout_when_hd_screens_are_off, "Save screen should be the PS1's when HD screens are turned off")
{
    s_hd_screens = 0;

    when_a_save_screen_opens();

    CHECK(!lom_draw_cards_on());
}

/**
 * @brief Given the Remaster's save screen pictures are missing the prompts' window; when a save screen opens; then it's
 *        the PS1's rather than a mix of the two.
 */
SPECIFICATION(keeps_the_ps1_layout_without_all_its_pictures, "Save screen should be the PS1's when the Remaster is missing any of its pictures")
{
    s_window[0].label = "not-the-window";

    when_a_save_screen_opens();

    CHECK(!lom_draw_cards_on());
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
