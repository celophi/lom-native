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
#define WORLD_SELECT_FILE REMASTER_FOLDER "/windata/map_select.psb.m"
#define QUAD_WORDS 9
#define WALL 0x604020FFu
#define SMALL_MAP 0x20A040FFu
#define FRAME 0xC0C000FFu
#define LAND 0x40A0E0FFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static RemasterPicture s_world_select_pictures[] = {
    {"map_select_bg", 1920, 1080, WALL, 0, NULL},
    {"map_select_map", 600, 540, SMALL_MAP, 0, NULL},
    {"worldmap_frame", 500, 500, FRAME, 0, NULL},
    {"worldmap_map", 1920, 1920, LAND, 0, NULL},
};
static int s_hd_screens = 1;
static POLY_FT4 s_quad;

/**
 * @brief Put in a fake of each provider world select's zoom uses, with the happy path's defaults: the Remaster in
 *        Steam's library with world select's art, and a renderer that can draw it.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Start the game with the launcher's settings, then have world select build the quad it zooms the small map
 *        with, part way into the zoom, hand it to the port and draw it, as WSEL does each frame of the zoom.
 */
static void when_world_select_draws_the_zoom(void)
{
    size_t size = 0;
    char hd_screens[8];
    unsigned char* art = remaster_psb_picture_file(s_world_select_pictures, 4, &size);
    remaster_files_provider_fake_add(&s_files_fake, WORLD_SELECT_FILE, art, size);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    lom_native_remaster_initialize();

    SetPolyFT4(&s_quad);
    setRGB0(&s_quad, 0x80, 0x80, 0x80);
    setXY4(&s_quad, 60, 30, 260, 30, 60, 200, 260, 200);
    s_quad.tpage = 0x2C;
    lom_draw_wsel_zoom(&s_quad.r0);
    gpu_draw(&s_quad.r0, QUAD_WORDS, sizeof(POLY_FT4), 1);
}

/**
 * @brief Given the defaults; when world select draws its zoom; then the Remaster's small map is stretched over the
 *        zooming quad's four corners, blended as the quad's page says.
 */
SPECIFICATION(stretches_the_small_map, "World select zoom should stretch the Remaster's small map over the zooming quad")
{
    when_world_select_draws_the_zoom();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == SMALL_MAP);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){60, 30, 260, 30, 60, 200, 260, 200}, sizeof(int[8])) == 0);
    CHECK(s_renderer_fake.pictures[0].tpage == 0x2C);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when world select draws its zoom; then the PS1's quad
 *        is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_zoom_when_hd_screens_are_off, "World select zoom should leave the PS1's zoom when HD screens are turned off")
{
    s_hd_screens = 0;

    when_world_select_draws_the_zoom();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_quad.code == 0x2C);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
