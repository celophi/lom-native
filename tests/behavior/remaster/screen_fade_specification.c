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

#define REMASTER_FOLDER "/steam/steamapps/common/Legend of Mana"
#define SPRITE_WORDS 4

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;
#define HALF_FADED 0x80
#define BLACK 0
#define NOT_FADED 0x100

static const char* s_remaster_folder = REMASTER_FOLDER;
static int s_hd_screens = 1;
static int s_whole_remaster = 1;
static int s_brightness = HALF_FADED;
static int s_drawn;
static SPRT s_sprite;

/**
 * @brief Put in a fake of each provider a screen's fade uses, with the happy path's defaults: the Remaster in Steam's
 *        library, and a renderer that can draw pictures.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Start the game with the launcher's settings and the player's choice of the whole Remaster in the in-game
 *        menu, then have a screen build its fade to black as a sprite over the screen, hand it to the port and, when
 *        the port draws it, draw it, as the screens do each frame of a fade.
 */
static void when_a_screen_fades(void)
{
    char hd_screens[8];
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    setenv(LOM_ENV_REMASTER_ROOT, s_remaster_folder, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    lom_native_remaster_initialize();
    lom_native_remaster_set_switch(LOM_REMASTER_SWITCH_ALL, s_whole_remaster);

    setSprt(&s_sprite);
    setRGB0(&s_sprite, 0x80, 0x80, 0x80);
    setXY0(&s_sprite, 0, 0);
    setWH(&s_sprite, 320, 240);
    s_drawn = lom_draw_fade(&s_sprite.r0, s_brightness, 0, 0, 320, 240);

    if (s_drawn)
    {
        gpu_draw(&s_sprite.r0, SPRITE_WORDS, sizeof(SPRT), 1);
    }
}

/**
 * @brief Given the defaults, a fade halfway to black; when a screen fades; then a black picture half see-through is
 *        drawn over the whole faded area, darkening every colour by half, rather than the PS1's fade, which takes the
 *        same amount off every colour.
 */
SPECIFICATION(darkens_in_proportion, "Screen fade should darken everything in proportion with a half see-through black when it's halfway")
{
    when_a_screen_fades();

    CHECK(s_drawn);
    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == 0x0000007Fu);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){0, 0, 320, 0, 0, 240, 320, 240}, sizeof(int[8])) == 0);
    CHECK(s_renderer_fake.pictures[0].rgb == 0x808080 && s_renderer_fake.pictures[0].semi == 0);
}

/**
 * @brief Given a fade all the way to black; when a screen fades; then a solid black picture is drawn over it.
 */
SPECIFICATION(goes_to_black, "Screen fade should draw solid black when it's all the way faded")
{
    s_brightness = BLACK;

    when_a_screen_fades();

    CHECK(s_renderer_fake.pictures[0].colour == 0x000000FFu);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when a screen fades halfway; then it still darkens in
 *        proportion, since the fade is the Remaster's on every screen, not one of its HD screens.
 */
SPECIFICATION(fades_without_hd_screens, "Screen fade should still darken in proportion when HD screens are turned off")
{
    s_hd_screens = 0;

    when_a_screen_fades();

    CHECK(s_drawn);
    CHECK(s_renderer_fake.pictures[0].colour == 0x0000007Fu);
}

/**
 * @brief Given the screen isn't faded at all; when a screen draws its fade; then nothing is drawn and the PS1's fade,
 *        which does nothing, is left.
 */
SPECIFICATION(draws_nothing_unfaded, "Screen fade should leave the PS1's fade when the screen isn't faded")
{
    s_brightness = NOT_FADED;

    when_a_screen_fades();

    CHECK(!s_drawn);
    CHECK(s_renderer_fake.picture_count == 0);
}

/**
 * @brief Given the player turned the whole Remaster off in the in-game menu; when a screen fades halfway; then the
 *        PS1's fade is left.
 */
SPECIFICATION(leaves_the_ps1_fade_with_the_remaster_off, "Screen fade should leave the PS1's fade when the whole Remaster is turned off")
{
    s_whole_remaster = 0;

    when_a_screen_fades();

    CHECK(!s_drawn);
    CHECK(s_sprite.code == 0x64);
}

/**
 * @brief Given the Remaster isn't installed; when a screen fades halfway; then the PS1's fade is left.
 */
SPECIFICATION(leaves_the_ps1_fade_without_the_remaster, "Screen fade should leave the PS1's fade when the Remaster isn't installed")
{
    s_remaster_folder = "/nowhere";

    when_a_screen_fades();

    CHECK(!s_drawn);
}

/**
 * @brief Given a renderer that can't draw pictures; when a screen fades halfway; then the PS1's fade is left.
 */
SPECIFICATION(leaves_the_ps1_fade_without_pictures, "Screen fade should leave the PS1's fade when the renderer can't draw pictures")
{
    hd_renderer_provider_fake_without_pictures(&s_renderer_fake);

    when_a_screen_fades();

    CHECK(!s_drawn);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
