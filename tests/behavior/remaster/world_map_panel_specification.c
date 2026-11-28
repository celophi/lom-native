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
#define DATA REMASTER_FOLDER "/windata/"
#define QUAD_WORDS 9
#define FIRST_PANEL 0x103010FFu
#define FOURTH_PANEL 0x406040FFu
#define LAST_PANEL 0x10A010FFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static RemasterPicture s_panels[] = {
    {"panel01", 400, 300, FIRST_PANEL, 0, NULL, 0, NULL},
    {"panel04", 400, 300, FOURTH_PANEL, 0, NULL, 0, NULL},
    {"panel10", 400, 300, LAST_PANEL, 0, NULL, 0, NULL},
};
static int s_hd_screens = 1;
static int s_text = LOM_REMASTER_TEXT_ENGLISH;
static int s_stage = 3;
static POLY_FT4 s_quad;

/**
 * @brief Put in a fake of each provider the world map's panel uses, with the happy path's defaults: the Remaster in Steam's library with
 *        its art, and a renderer that can draw it.
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
 * @brief Start the game with the launcher's settings.
 */
static void start_the_game(void)
{
    char hd_screens[8], text[8];
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    snprintf(text, sizeof(text), "%d", s_text);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    setenv(LOM_ENV_REMASTER_TEXT, text, 1);
    lom_native_remaster_initialize();
}

/**
 * @brief Start the game, then have the world map build its panel's quad for the Mana Tree's growth, hand it to the
 *        port and draw it, as WMAP does each frame.
 */
static void when_the_world_map_draws_its_panel(void)
{
    the_remaster_has(DATA "mana_panel.psb.m", s_panels, 3);
    start_the_game();

    SetPolyFT4(&s_quad);
    setRGB0(&s_quad, 0x80, 0x80, 0x80);
    setXY4(&s_quad, 8, 150, 97, 150, 8, 217, 97, 217);
    s_quad.tpage = 0x0B;
    lom_draw_wmap_panel(&s_quad.r0, s_stage);
    gpu_draw(&s_quad.r0, QUAD_WORDS, sizeof(POLY_FT4), 1);
}

/**
 * @brief Given the defaults, the tree at its fourth stage; when the world map draws its panel; then the Remaster's
 *        fourth panel covers the PS1's panel's four points.
 */
SPECIFICATION(draws_the_panel_for_the_trees_growth, "World map panel should draw the Remaster's panel for the Mana Tree's growth")
{
    when_the_world_map_draws_its_panel();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == FOURTH_PANEL);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){8, 150, 97, 150, 8, 217, 97, 217}, sizeof(int[8])) == 0);
    CHECK(s_renderer_fake.pictures[0].tpage == 0x0B);
}

/**
 * @brief Given the tree has grown past the Remaster's last panel; when the world map draws its panel; then the
 *        Remaster's last panel is drawn.
 */
SPECIFICATION(draws_the_last_panel_past_it, "World map panel should draw the Remaster's last panel when the tree has grown past it")
{
    s_stage = 12;

    when_the_world_map_draws_its_panel();

    CHECK(s_renderer_fake.pictures[0].colour == LAST_PANEL);
}

/**
 * @brief Given the tree hasn't grown; when the world map draws its panel; then the Remaster's first panel is drawn.
 */
SPECIFICATION(draws_the_first_panel_before_growth, "World map panel should draw the Remaster's first panel before the tree has grown")
{
    s_stage = 0;

    when_the_world_map_draws_its_panel();

    CHECK(s_renderer_fake.pictures[0].colour == FIRST_PANEL);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the world map draws its panel; then the PS1's
 *        panel is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_panel_when_hd_screens_are_off, "World map panel should leave the PS1's panel when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_world_map_draws_its_panel();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_quad.code == 0x2C);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
