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
#define SPRITE_WORDS 4
#define LAND_MAP 0
#define WORLD_MAP 1
#define CURSOR 2
#define HERO 4
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
static int s_picture = WORLD_MAP;
static int s_shadow = 0;
static int s_x = 0;
static int s_y = 0;
static int s_sprite_rect[4] = {0, 0, 128, 224};
static SPRT s_sprite;

/**
 * @brief Put in a fake of each provider world select's pictures use, with the happy path's defaults: the Remaster in
 *        Steam's library with world select's art, and a renderer that can draw it.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Start the game with the launcher's settings, then have world select build one sprite of one of its pictures,
 *        hand it to the port and draw it, as WSEL does each frame.
 */
static void when_world_select_draws_a_sprite(void)
{
    size_t size = 0;
    char hd_screens[8];
    unsigned char* art = remaster_psb_picture_file(s_world_select_pictures, 4, &size);
    remaster_files_provider_fake_add(&s_files_fake, WORLD_SELECT_FILE, art, size);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    lom_native_remaster_initialize();

    setSprt(&s_sprite);
    setRGB0(&s_sprite, 0x80, 0x80, 0x80);
    setXY0(&s_sprite, s_sprite_rect[0], s_sprite_rect[1]);
    setWH(&s_sprite, s_sprite_rect[2], s_sprite_rect[3]);
    lom_draw_wsel_picture(&s_sprite.r0, sizeof(SPRT), 1, s_picture, s_shadow, s_x, s_y);
    gpu_draw(&s_sprite.r0, SPRITE_WORDS, sizeof(SPRT), 1);
}

/**
 * @brief Given the defaults, the world map's left sprite; when world select draws it; then the Remaster's wall is
 *        drawn over the sprite, and the part of the Remaster's small map the sprite covers is drawn over the wall, where
 *        the PS1's small map is.
 */
SPECIFICATION(draws_the_wall_and_the_small_map, "World select picture should draw the Remaster's wall with its small map over it for the world map")
{
    when_world_select_draws_a_sprite();

    CHECK(s_renderer_fake.picture_count == 2);
    CHECK(s_renderer_fake.pictures[0].colour == WALL);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){0, 0, 128, 0, 0, 224, 128, 224}, sizeof(int[8])) == 0);
    CHECK(s_renderer_fake.pictures[1].colour == SMALL_MAP);
    CHECK(memcmp(s_renderer_fake.pictures[1].xy, (int[8]){96, 48, 128, 48, 96, 168, 128, 168}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the world map's sprite at its right, past the small map; when world select draws it; then only the
 *        Remaster's wall is drawn there.
 */
SPECIFICATION(draws_only_the_wall_past_the_small_map, "World select picture should draw only the Remaster's wall where the small map isn't")
{
    s_sprite_rect[0] = 256;
    s_sprite_rect[2] = 64;

    when_world_select_draws_a_sprite();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == WALL);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){256, 0, 320, 0, 256, 224, 320, 224}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the land map's sprite, scrolled up and left; when world select draws it; then the Remaster's land map
 *        is drawn over it.
 */
SPECIFICATION(draws_the_land_map, "World select picture should draw the Remaster's land map for the land map")
{
    s_picture = LAND_MAP;
    s_x = -64;
    s_y = -112;
    memcpy(s_sprite_rect, (int[4]){-64, -112, 128, 256}, sizeof(s_sprite_rect));

    when_world_select_draws_a_sprite();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == LAND);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){-64, -112, 64, -112, -64, 144, 64, 144}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the cursor's sprite; when world select draws it; then the Remaster's frame is drawn over it.
 */
SPECIFICATION(draws_the_cursor_frame, "World select picture should draw the Remaster's frame for the cursor")
{
    s_picture = CURSOR;
    s_x = 101;
    s_y = 53;
    memcpy(s_sprite_rect, (int[4]){101, 53, 120, 118}, sizeof(s_sprite_rect));

    when_world_select_draws_a_sprite();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == FRAME);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){101, 53, 221, 53, 101, 171, 221, 171}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the cursor's drop shadow; when world select draws it; then nothing is drawn for it, since the
 *        Remaster's frame has its own, and the PS1's shadow isn't drawn either.
 */
SPECIFICATION(drops_the_cursor_shadow, "World select picture should draw no shadow under the Remaster's cursor")
{
    s_picture = CURSOR;
    s_shadow = 1;
    memcpy(s_sprite_rect, (int[4]){105, 57, 120, 118}, sizeof(s_sprite_rect));

    when_world_select_draws_a_sprite();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprite.code != 0x64);
}

/**
 * @brief Given the hero's sprite, which the Remaster has no art for; when world select draws it; then the PS1's hero is
 *        left as the game built it.
 */
SPECIFICATION(leaves_the_hero_to_the_ps1, "World select picture should leave the PS1's hero")
{
    s_picture = HERO;
    memcpy(s_sprite_rect, (int[4]){216, 180, 32, 40}, sizeof(s_sprite_rect));

    when_world_select_draws_a_sprite();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprite.code == 0x64);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when world select draws the world map; then the PS1's
 *        sprite is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_when_hd_screens_are_off, "World select picture should leave the PS1's pictures when HD screens are turned off")
{
    s_hd_screens = 0;

    when_world_select_draws_a_sprite();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprite.code == 0x64);
}

/**
 * @brief Given the Remaster's world select art is missing its small map; when world select draws the world map; then
 *        the PS1's world map is left whole rather than drawing the Remaster's wall without its map.
 */
SPECIFICATION(leaves_the_ps1_world_map_without_the_small_map, "World select picture should leave the PS1's world map when the Remaster is missing its small map")
{
    s_world_select_pictures[1].label = "not-the-map";

    when_world_select_draws_a_sprite();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprite.code == 0x64);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
