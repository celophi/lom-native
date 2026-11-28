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
#define SALAMANDER 4
#define GNOME 5
#define ICON 24
#define ORB 16
#define GNOME_FRAME_5 0xA08040FFu
#define GNOME_FULL 0xC0A060FFu
#define SALAMANDER_FULL 0xE04020FFu
#define EMPTY_ORB 0xFFFFFFFFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static RemasterPicture s_animations[] = {{"gnome5", 120, 120, GNOME_FRAME_5, 0, NULL, 0, NULL}};
static RemasterPicture s_meters[] = {
    {"zero", 128, 128, EMPTY_ORB, 0, NULL, 0, NULL},
    {"gnome3m", 128, 128, GNOME_FULL, 0, NULL, 0, NULL},
    {"salamander3", 128, 128, SALAMANDER_FULL, 0, NULL, 0, NULL},
};
static int s_hd_screens = 1;
static int s_text = LOM_REMASTER_TEXT_ENGLISH;
static int s_spirit = GNOME;
static int s_kind = LOM_REMASTER_WMAP_SPIRIT_ICON;
static int s_u = 4 * ICON;
static int s_v = GNOME * ICON;
static int s_size = ICON;
static SPRT s_sprite;

/**
 * @brief Put in a fake of each provider the world map's spirits use, with the happy path's defaults: the Remaster in Steam's library with
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
 * @brief Start the game, then have the world map build a spirit's sprite, or the sprite of its strength's orb, from
 *        the PS1's sheet, hand it to the port and draw it, as WMAP does each frame.
 */
static void when_the_world_map_draws_a_spirit(void)
{
    the_remaster_has(DATA "spirit_anime.psb.m", s_animations, 1);
    the_remaster_has(DATA "spirit_meter.psb.m", s_meters, 3);
    start_the_game();

    setSprt(&s_sprite);
    setRGB0(&s_sprite, 0x80, 0x80, 0x80);
    setXY0(&s_sprite, 200, 100);
    setWH(&s_sprite, s_size, s_size);
    setUV0(&s_sprite, s_u, s_v);
    lom_draw_wmap_spirit(&s_sprite.r0, s_spirit, s_kind);
    gpu_draw(&s_sprite.r0, SPRITE_WORDS, sizeof(SPRT), 1);
}

/**
 * @brief Given the defaults, the PS1 showing the last of Gnome's five frames; when the world map draws Gnome; then the
 *        frame drawn is the Remaster's fifth of its six, spreading the PS1's frames across the Remaster's, over where
 *        the PS1's spirit is in its sprite.
 */
SPECIFICATION(draws_the_matching_frame, "World map spirit should draw the Remaster's frame that matches the one the PS1 shows")
{
    when_the_world_map_draws_a_spirit();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == GNOME_FRAME_5);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){202, 102, 222, 102, 202, 122, 222, 122}, sizeof(int[8])) == 0);
}

/**
 * @brief Given Gnome is at its strongest, the PS1's full orb; when the world map draws Gnome's strength; then the
 *        Remaster's full orb in Gnome's colour is drawn over where the PS1's orb is in its sprite.
 */
SPECIFICATION(draws_the_full_orb, "World map spirit should draw the Remaster's full orb in the spirit's colour when it's at its strongest")
{
    s_kind = LOM_REMASTER_WMAP_SPIRIT_LEVEL;
    s_size = ORB;
    s_u = 128 + 3 * ORB;
    s_v = 3 * ORB;

    when_the_world_map_draws_a_spirit();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == GNOME_FULL);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){202, 102, 215, 102, 202, 114, 215, 114}, sizeof(int[8])) == 0);
}

/**
 * @brief Given Salamander at its strongest; when the world map draws its strength; then the Remaster's full Salamander
 *        orb is drawn, which the Remaster names differently from the others.
 */
SPECIFICATION(draws_salamanders_full_orb, "World map spirit should draw Salamander's full orb when Salamander is at its strongest")
{
    s_spirit = SALAMANDER;
    s_kind = LOM_REMASTER_WMAP_SPIRIT_LEVEL;
    s_size = ORB;
    s_u = 128 + 3 * ORB;
    s_v = 3 * ORB;

    when_the_world_map_draws_a_spirit();

    CHECK(s_renderer_fake.quads[0].colour == SALAMANDER_FULL);
}

/**
 * @brief Given Gnome is weak, the PS1's darker empty orb; when the world map draws Gnome's strength; then the
 *        Remaster's empty orb is drawn darkened the same way, since the Remaster has no art of its own for it.
 */
SPECIFICATION(darkens_the_empty_orb, "World map spirit should darken the Remaster's empty orb when the spirit is weak")
{
    s_kind = LOM_REMASTER_WMAP_SPIRIT_LEVEL;
    s_size = ORB;
    s_u = 128 + 2 * ORB;
    s_v = 0;

    when_the_world_map_draws_a_spirit();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == EMPTY_ORB);
    CHECK(s_renderer_fake.quads[0].rgb == 0x404040);
}

/**
 * @brief Given an orb the game never uses; when the world map draws it; then the PS1's sprite is left as the game built
 *        it.
 */
SPECIFICATION(leaves_unused_orbs_to_the_ps1, "World map spirit should leave the PS1's orb when it's one the game doesn't use")
{
    s_kind = LOM_REMASTER_WMAP_SPIRIT_LEVEL;
    s_size = ORB;
    s_u = 128;
    s_v = ORB;

    when_the_world_map_draws_a_spirit();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_sprite.code == 0x64);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the world map draws Gnome; then the PS1's sprite
 *        is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_spirit_when_hd_screens_are_off, "World map spirit should leave the PS1's spirit when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_world_map_draws_a_spirit();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_sprite.code == 0x64);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
