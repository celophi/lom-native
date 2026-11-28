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

static SPRT s_sprites[2];

/**
 * @brief Open the naming screen, then have it build its background as two sprites of the PS1's, hand them to the port
 *        and draw them, as GNAME does each frame.
 */
static void when_the_naming_screen_draws_its_background(void)
{
    open_the_naming_screen();

    for (int i = 0; i < 2; ++i)
    {
        setSprt(&s_sprites[i]);
        setRGB0(&s_sprites[i], 0x80, 0x80, 0x80);
        setXY0(&s_sprites[i], i * 160, 0);
        setWH(&s_sprites[i], 160, 224);
    }

    lom_draw_gname_background(&s_sprites[0].r0, sizeof(SPRT), 2);
    gpu_draw(&s_sprites[0].r0, SPRITE_WORDS, sizeof(SPRT), 2);
}

/**
 * @brief Given the defaults; when the naming screen draws its background; then the Remaster's English background, with
 *        its buttons painted in, is drawn once over the whole screen, and the PS1's background sprites draw nothing.
 */
SPECIFICATION(draws_the_background_once, "Naming screen background should draw the Remaster's background once over the whole screen")
{
    when_the_naming_screen_draws_its_background();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == ENGLISH_BACKGROUND);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){0, 0, 320, 0, 0, 224, 320, 224}, sizeof(int[8])) == 0);
    CHECK(s_sprites[1].code != 0x64);
}

/**
 * @brief Given the Remaster's text is in German; when the naming screen draws its background; then the Remaster's
 *        German background is drawn, with its buttons' German words.
 */
SPECIFICATION(draws_the_german_background, "Naming screen background should draw the German background when the Remaster's text is in German")
{
    s_text = LOM_REMASTER_TEXT_GERMAN;

    when_the_naming_screen_draws_its_background();

    CHECK(s_renderer_fake.quads[0].colour == GERMAN_BACKGROUND);
}

/**
 * @brief Given Widescreen is off, so the naming screen keeps the PS1's layout; when it draws its background; then the
 *        PS1's sprites are left as the game built them.
 */
SPECIFICATION(leaves_the_ps1_background_in_the_ps1_layout, "Naming screen background should leave the PS1's background in the PS1's layout")
{
    s_widescreen = 0;

    when_the_naming_screen_draws_its_background();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_sprites[0].code == 0x64 && s_sprites[1].code == 0x64);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
