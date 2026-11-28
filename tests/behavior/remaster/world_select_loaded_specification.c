#include "lom_native_draw.h"
#include "lom_native_env.h"
#include "lom_native_remaster.h"

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
#define WORLD_SELECT_FILE REMASTER_FOLDER "/windata/map_select.psb.m"
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

/**
 * @brief Put in a fake of each provider loading world select uses, with the happy path's defaults: the Remaster in
 *        Steam's library with world select's art, and a renderer that can draw it.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Start the game with the launcher's settings, then have world select load, as WSEL does before its first
 *        frame.
 */
static void when_world_select_loads(void)
{
    size_t size = 0;
    char hd_screens[8];
    unsigned char* art = remaster_psb_picture_file(s_world_select_pictures, 4, &size);
    remaster_files_provider_fake_add(&s_files_fake, WORLD_SELECT_FILE, art, size);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    lom_native_remaster_initialize();
    lom_draw_wsel_loaded();
}

/**
 * @brief Given the defaults; when world select loads; then all four of the Remaster's world select pictures are given
 *        to the renderer before anything is drawn, reading the Remaster's file once, so its first frames don't wait on
 *        them.
 */
SPECIFICATION(loads_the_art_up_front, "World select should give the renderer all its Remaster art when it loads")
{
    when_world_select_loads();

    CHECK(s_renderer_fake.upload_count == 4);
    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(remaster_files_provider_fake_find(&s_files_fake, WORLD_SELECT_FILE)->reads == 1);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when world select loads; then nothing is read or given
 *        to the renderer.
 */
SPECIFICATION(loads_nothing_when_hd_screens_are_off, "World select should load none of the Remaster's art when HD screens are turned off")
{
    s_hd_screens = 0;

    when_world_select_loads();

    CHECK(s_renderer_fake.upload_count == 0);
    CHECK(remaster_files_provider_fake_find(&s_files_fake, WORLD_SELECT_FILE)->reads == 0);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
