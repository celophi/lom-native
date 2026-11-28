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
#define DAY_NAME 0
#define SHELL 1
#define SPIRIT 2
#define SHELL_COLOUR 0x806040FFu
#define JINN 0x40E0E0FFu
#define DRYAD 0x40C040FFu
#define GERMAN_NAMES 0x7070F0FFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static const uint32_t s_english_day_names[6] = {0x100000FFu, 0x200000FFu, 0x300000FFu, 0x400000FFu, 0x500000FFu, 0x600000FFu};
static RemasterPicture s_english_names[] = {{"day_names", 432, 864, 0, 0, NULL, 6, s_english_day_names}};
static RemasterPicture s_german_names[] = {{"day_names", 432, 864, GERMAN_NAMES, 0, NULL, 0, NULL}};
static RemasterPicture s_kameo[] = {
    {"none_k", 260, 300, SHELL_COLOUR, 0, NULL, 0, NULL},
    {"jinn_k", 260, 300, JINN, 0, NULL, 0, NULL},
    {"dryad_k", 260, 300, DRYAD, 0, NULL, 0, NULL},
};
static int s_hd_screens = 1;
static int s_text = LOM_REMASTER_TEXT_ENGLISH;
static int s_day = 3;
static unsigned char s_spirit_shade = 0x80;
static int s_spirit_fading = 0;
static SPRT s_sprites[3];

/**
 * @brief Put in a fake of each provider the world map's status corner uses, with the happy path's defaults: the
 *        Remaster in Steam's library with the day names and the spirits in their shells, and a renderer that can draw
 *        them.
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
 * @brief Start the game with the launcher's settings, then have the world map build its status corner as the PS1
 *        does, from its sprite sheets: the day's name from its row of the names, the empty shell, and the day's spirit
 *        from its cell, then hand each to the port and draw them in that order, as WMAP does each frame.
 */
static void when_the_world_map_draws_its_status(void)
{
    char hd_screens[8], text[8];
    the_remaster_has(DATA "system/image/hiduke_en.psb.m", s_english_names, 1);
    the_remaster_has(DATA "system/image/hiduke_ge.psb.m", s_german_names, 1);
    the_remaster_has(DATA "kameo.psb.m", s_kameo, 3);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    snprintf(text, sizeof(text), "%d", s_text);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    setenv(LOM_ENV_REMASTER_TEXT, text, 1);
    lom_native_remaster_initialize();

    for (int i = 0; i < 3; ++i)
    {
        setSprt(&s_sprites[i]);
        setRGB0(&s_sprites[i], 0x80, 0x80, 0x80);
    }

    setXY0(&s_sprites[DAY_NAME], 168, 16);
    setWH(&s_sprites[DAY_NAME], 96, 32);
    setUV0(&s_sprites[DAY_NAME], 0, s_day * 32);
    setXY0(&s_sprites[SHELL], 262, 14);
    setWH(&s_sprites[SHELL], 48, 56);
    setUV0(&s_sprites[SHELL], 0, 168);
    setXY0(&s_sprites[SPIRIT], 270, 28);
    setWH(&s_sprites[SPIRIT], 33, 28);
    setUV0(&s_sprites[SPIRIT], s_day % 2 * 48 + 8, s_day / 2 * 56 + 14);
    setRGB0(&s_sprites[SPIRIT], s_spirit_shade, s_spirit_shade, s_spirit_shade);
    setSemiTrans(&s_sprites[SPIRIT], s_spirit_fading);

    lom_draw_wmap_status(&s_sprites[DAY_NAME].r0, LOM_REMASTER_WMAP_DAY_NAME);
    lom_draw_wmap_status(&s_sprites[SHELL].r0, LOM_REMASTER_WMAP_SHELL);
    lom_draw_wmap_status(&s_sprites[SPIRIT].r0, LOM_REMASTER_WMAP_DAY_ICON);
    gpu_draw(&s_sprites[0].r0, SPRITE_WORDS, sizeof(SPRT), 3);
}

/**
 * @brief Given the defaults, the fourth day; when the world map draws its status; then the fourth day's name is drawn
 *        from its row of the Remaster's English day names, over the PS1's name and 4 pixels lower, onto the glow
 *        behind it.
 */
SPECIFICATION(draws_the_days_name, "World map status should draw the day's name from its row of the Remaster's day names")
{
    when_the_world_map_draws_its_status();

    CHECK(s_renderer_fake.quads[0].colour == s_english_day_names[3]);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){168, 20, 264, 20, 168, 52, 264, 52}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the Remaster's text is in German; when the world map draws its status; then the day's name comes from
 *        the Remaster's German day names.
 */
SPECIFICATION(draws_the_days_name_in_german, "World map status should draw the day's name in German when the Remaster's text is in German")
{
    s_text = LOM_REMASTER_TEXT_GERMAN;

    when_the_world_map_draws_its_status();

    CHECK(s_renderer_fake.quads[0].colour == GERMAN_NAMES);
}

/**
 * @brief Given the defaults, the fourth day, Jinn's; when the world map draws its status; then the Remaster's shell is
 *        drawn, then Jinn over it rather than under it, since the Remaster's shell is solid inside.
 */
SPECIFICATION(draws_the_spirit_over_its_shell, "World map status should draw the day's spirit over the Remaster's shell")
{
    when_the_world_map_draws_its_status();

    CHECK(s_renderer_fake.quad_count == 3);
    CHECK(s_renderer_fake.quads[1].colour == SHELL_COLOUR);
    CHECK(memcmp(s_renderer_fake.quads[1].xy, (int[8]){262, 14, 310, 14, 262, 70, 310, 70}, sizeof(int[8])) == 0);
    CHECK(s_renderer_fake.quads[2].colour == JINN);
    CHECK(memcmp(s_renderer_fake.quads[2].xy, (int[8]){270, 28, 303, 28, 270, 56, 303, 56}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the fifth day, Dryad's; when the world map draws its status; then the spirit drawn is Dryad.
 */
SPECIFICATION(draws_the_days_own_spirit, "World map status should draw the spirit of the day it is")
{
    s_day = 4;

    when_the_world_map_draws_its_status();

    CHECK(s_renderer_fake.quads[2].colour == DRYAD);
}

/**
 * @brief Given the spirit is fading in, darker and blended; when the world map draws its status; then the Remaster's
 *        spirit is drawn as dark and as blended as the PS1's.
 */
SPECIFICATION(fades_the_spirit_as_the_ps1_does, "World map status should fade the Remaster's spirit as the PS1 fades its own")
{
    s_spirit_shade = 0x40;
    s_spirit_fading = 1;

    when_the_world_map_draws_its_status();

    CHECK(s_renderer_fake.quads[2].rgb == 0x404040);
    CHECK(s_renderer_fake.quads[2].alpha == 0x80);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the world map draws its status; then nothing of
 *        the Remaster's is drawn and the PS1's sprites are left as the game built them.
 */
SPECIFICATION(leaves_the_ps1_status_when_hd_screens_are_off, "World map status should leave the PS1's status when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_world_map_draws_its_status();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_sprites[DAY_NAME].code == 0x64 && s_sprites[SHELL].code == 0x64 && s_sprites[SPIRIT].code == 0x64);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
