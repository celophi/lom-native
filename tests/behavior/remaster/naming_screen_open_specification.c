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

static int s_cursor_x;
static int s_cursor_y;

/**
 * @brief Open the naming screen, then ask where its cursor goes on its second button and how far its tabs move, as
 *        GNAME does as it lays the screen out.
 */
static void when_the_naming_screen_opens(void)
{
    open_the_naming_screen();
    s_cursor_x = 100;
    s_cursor_y = 60;
    lom_draw_gname_cursor(3, &s_cursor_x, &s_cursor_y);
}

/**
 * @brief Given the defaults, Widescreen and HD screens on; when the naming screen opens; then it's laid out as the
 *        Remaster's: its buttons in a column at the left, where the cursor goes to the second one, and its tabs moved
 *        60 pixels left.
 */
SPECIFICATION(uses_the_remasters_layout, "Naming screen should be laid out as the Remaster's when Widescreen and HD screens are on")
{
    when_the_naming_screen_opens();

    CHECK(lom_draw_gname_column());
    CHECK(s_cursor_x == 41 && s_cursor_y == 119);
    CHECK(lom_draw_gname_tab_shift() == -60);
}

/**
 * @brief Given the player has Widescreen off, which the Remaster's wider naming screen needs; when the naming screen
 *        opens; then it keeps the PS1's layout, its cursor where the game put it.
 */
SPECIFICATION(keeps_the_ps1_layout_without_widescreen, "Naming screen should keep the PS1's layout when Widescreen is off")
{
    s_widescreen = 0;

    when_the_naming_screen_opens();

    CHECK(!lom_draw_gname_column());
    CHECK(s_cursor_x == 100 && s_cursor_y == 60);
    CHECK(lom_draw_gname_tab_shift() == 0);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the naming screen opens; then it keeps the PS1's
 *        layout.
 */
SPECIFICATION(keeps_the_ps1_layout_when_hd_screens_are_off, "Naming screen should keep the PS1's layout when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_naming_screen_opens();

    CHECK(!lom_draw_gname_column());
}

/**
 * @brief Given the Remaster's naming screen art is missing one of the typing hands; when the naming screen opens; then
 *        it keeps the PS1's layout rather than mixing the two.
 */
SPECIFICATION(keeps_the_ps1_layout_without_all_the_art, "Naming screen should keep the PS1's layout when the Remaster is missing any of its art")
{
    s_naming_pictures[9].label = "not-a-hand";

    when_the_naming_screen_opens();

    CHECK(!lom_draw_gname_column());
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
