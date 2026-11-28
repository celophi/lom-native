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
#define SPRITE_WORDS 4
#define LAND 2
#define ARTIFACT 64
#define CLEARED_CLUT_ROW 506
#define NOT_CLEARED_CLUT_ROW 504
#define NAME_PAGE 0x15
#define CLEARED_BANNER 0x20C020FFu
#define NOT_CLEARED_BANNER 0xE0E020FFu
#define ARTIFACT_BANNER 0xC060E0FFu
#define JAPANESE_BANNER 0xE02020FFu

void lom_gpu_image_loaded(int x, int y, int w, int h, const void* data);

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static RemasterPicture s_cleared[] = {{"banner", 360, 90, CLEARED_BANNER, 0, NULL, 0, NULL}};
static RemasterPicture s_not_cleared[] = {{"banner", 360, 90, NOT_CLEARED_BANNER, 0, NULL, 0, NULL}};
static RemasterPicture s_artifact[] = {{"banner", 360, 90, ARTIFACT_BANNER, 0, NULL, 0, NULL}};
static RemasterPicture s_japanese[] = {{"banner", 360, 90, JAPANESE_BANNER, 0, NULL, 0, NULL}};
static int s_hd_screens = 1;
static int s_text = LOM_REMASTER_TEXT_ENGLISH;
static int s_selection = LAND;
static int s_clut_row = CLEARED_CLUT_ROW;
static uint16_t s_name_texture[16][16];
static SPRT s_sprite;

/**
 * @brief Put in a fake of each provider the world map's land names use, with the happy path's defaults: the Remaster in Steam's library with
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
 * @brief Start the game, have the game load the PS1's name texture, with the name drawn only in its middle, then build
 *        the name's sprite, hand it to the port and draw it, as WMAP does when a land is chosen.
 */
static void when_the_world_map_draws_a_lands_name(void)
{
    the_remaster_has(DATA "system/image/land_name/US/map_landname_0003.psb.m", s_cleared, 1);
    the_remaster_has(DATA "system/image/land_name/US/map_landname_c_0003.psb.m", s_not_cleared, 1);
    the_remaster_has(DATA "system/image/land_name/US/map_artifactname_0003.psb.m", s_artifact, 1);
    the_remaster_has(DATA "system/image/land_name/JP/map_landname_0003.psb.m", s_japanese, 1);
    start_the_game();

    for (int row = 4; row < 12; ++row)
    {
        for (int word = 4; word < 12; ++word)
        {
            s_name_texture[row][word] = 0x1111;
        }
    }

    lom_gpu_image_loaded(320, 256, 16, 16, s_name_texture);
    setSprt(&s_sprite);
    setRGB0(&s_sprite, 0x80, 0x80, 0x80);
    setSemiTrans(&s_sprite, 1);
    setXY0(&s_sprite, 100, 50);
    setWH(&s_sprite, 64, 16);
    setUV0(&s_sprite, 0, 0);
    s_sprite.clut = (unsigned short)(s_clut_row << 6);
    lom_draw_wmap_label(&s_sprite.r0, s_selection, NAME_PAGE);
    gpu_draw(&s_sprite.r0, SPRITE_WORDS, sizeof(SPRT), 1);
}

/**
 * @brief Given the defaults, a cleared land; when the world map draws its name; then the Remaster's banner for a cleared
 *        land is drawn at two ninths of its size, centred on where the PS1's name has anything drawn, over what's under
 *        it rather than added to it.
 */
SPECIFICATION(draws_the_cleared_banner, "World map label should draw the Remaster's cleared banner centred on the PS1's name when the land is cleared")
{
    when_the_world_map_draws_a_lands_name();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == CLEARED_BANNER);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){92, 48, 172, 48, 92, 68, 172, 68}, sizeof(int[8])) == 0);
    CHECK(s_renderer_fake.pictures[0].semi == 0);
}

/**
 * @brief Given the land isn't cleared yet; when the world map draws its name; then the Remaster's other banner is
 *        drawn.
 */
SPECIFICATION(draws_the_not_cleared_banner, "World map label should draw the Remaster's other banner when the land isn't cleared")
{
    s_clut_row = NOT_CLEARED_CLUT_ROW;

    when_the_world_map_draws_a_lands_name();

    CHECK(s_renderer_fake.pictures[0].colour == NOT_CLEARED_BANNER);
}

/**
 * @brief Given the name is the land's artifact's; when the world map draws it; then the Remaster's banner for that
 *        artifact is drawn.
 */
SPECIFICATION(draws_the_artifacts_banner, "World map label should draw the artifact's banner for an artifact's name")
{
    s_selection = ARTIFACT + LAND;

    when_the_world_map_draws_a_lands_name();

    CHECK(s_renderer_fake.pictures[0].colour == ARTIFACT_BANNER);
}

/**
 * @brief Given the Remaster's text is in Japanese; when the world map draws a land's name; then the banner comes from
 *        the Remaster's Japanese names.
 */
SPECIFICATION(draws_the_japanese_banner, "World map label should draw the Japanese banner when the Remaster's text is in Japanese")
{
    s_text = LOM_REMASTER_TEXT_JAPANESE;

    when_the_world_map_draws_a_lands_name();

    CHECK(s_renderer_fake.pictures[0].colour == JAPANESE_BANNER);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the world map draws a land's name; then the PS1's
 *        name is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_name_when_hd_screens_are_off, "World map label should leave the PS1's name when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_world_map_draws_a_lands_name();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprite.code == 0x66);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
