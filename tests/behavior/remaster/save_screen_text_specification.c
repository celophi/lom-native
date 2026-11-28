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

static const char* s_words = "Day 3";
static int s_style = LOM_CARDS_TEXT_ROW;
static int s_align = LOM_CARDS_ALIGN_LEFT;
static int s_written;
static SPRT s_sprite;

/**
 * @brief Open a save screen, then have it build a sprite for some of its words, hand it to the port with the words, in
 *        the game's text, and where they go, and draw it, as the save screens do each frame.
 */
static void when_a_save_screen_writes_its_words(void)
{
    open_a_save_screen();
    setSprt(&s_sprite);
    setRGB0(&s_sprite, 0x80, 0x80, 0x80);
    setXY0(&s_sprite, 60, 80);
    setWH(&s_sprite, 8, 8);
    s_written = lom_draw_cards_text(&s_sprite.r0, (const unsigned char*)s_words, 60, 100, s_align, s_style);

    if (s_written)
    {
        gpu_draw(&s_sprite.r0, SPRITE_WORDS, sizeof(SPRT), 1);
    }
}

/**
 * @brief Given the defaults, a row's words; when a save screen writes them; then they're written in the Remaster's
 *        font, in white, from where the PS1's start and on the PS1's baseline, a 4.5th of the font's size, its shadow
 *        and all.
 */
SPECIFICATION(writes_in_the_remasters_font, "Save screen text should write a row's words in white in the Remaster's font on the PS1's baseline")
{
    when_a_save_screen_writes_its_words();

    CHECK(s_written);
    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == hd_renderer_fake_written(0xFFFFFF));
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){60, 91, 111, 91, 60, 103, 111, 103}, sizeof(int[8])) == 0);
}

/**
 * @brief Given words aligned right; when a save screen writes them; then they end where the PS1's would.
 */
SPECIFICATION(ends_right_aligned_words_there, "Save screen text should end right-aligned words where the PS1's end")
{
    s_align = LOM_CARDS_ALIGN_RIGHT;

    when_a_save_screen_writes_its_words();

    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){9, 91, 60, 91, 9, 103, 60, 103}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the words of the card that isn't chosen; when a save screen writes them; then they're a dimmer brown.
 */
SPECIFICATION(dims_the_card_not_chosen, "Save screen text should write the card that isn't chosen in a dimmer brown")
{
    s_style = LOM_CARDS_TEXT_DIM;

    when_a_save_screen_writes_its_words();

    CHECK(s_renderer_fake.quads[0].colour == hd_renderer_fake_written(0x8A7848));
}

/**
 * @brief Given words with a letter the Remaster's font doesn't have; when a save screen writes them; then the PS1's
 *        words are left, rather than writing some of them in each font.
 */
SPECIFICATION(leaves_words_the_font_cant_write, "Save screen text should leave the PS1's words when the Remaster's font can't write them")
{
    s_words = "Day 4";

    when_a_save_screen_writes_its_words();

    CHECK(!s_written);
    CHECK(s_renderer_fake.quad_count == 0);
}

/**
 * @brief Given Widescreen is off, so the save screen is the PS1's; when it writes a row's words; then the PS1's words
 *        are left.
 */
SPECIFICATION(leaves_the_ps1_words, "Save screen text should leave the PS1's words when the screen is the PS1's")
{
    s_widescreen = 0;

    when_a_save_screen_writes_its_words();

    CHECK(!s_written);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
