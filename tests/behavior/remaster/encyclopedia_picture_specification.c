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
#define SPRITE_WORDS 4

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;
#define FIRST_PAGE 0x0BFD
#define NOT_A_PAGE 0x0001
#define PAINTING 0x8090A0FFu

static RemasterPicture s_painting[] = {{"jt0001", 500, 600, PAINTING, 0, NULL, 0, NULL}};
static int s_hd_screens = 1;
static int s_page = FIRST_PAGE;
static SPRT s_sprites[3];

/**
 * @brief Put in a fake of each provider the encyclopedia's pictures use, with the happy path's defaults: the
 *        Remaster in Steam's library with its first encyclopedia painting, and a renderer that can draw it.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Start the game with the launcher's settings, then have the encyclopedia build a page's picture as three
 *        sprites of the PS1's, hand them to the port with the page's file, and draw them, as the encyclopedia does each
 *        frame.
 */
static void when_the_encyclopedia_draws_a_pages_picture(void)
{
    size_t size = 0;
    char hd_screens[8];
    unsigned char* file = remaster_psb_picture_file(s_painting, 1, &size);
    remaster_files_provider_fake_add(&s_files_fake, REMASTER_FOLDER "/windata/system/image/zukan/jt0001.psb.m", file, size);
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    lom_native_remaster_initialize();

    for (int i = 0; i < 3; ++i)
    {
        setSprt(&s_sprites[i]);
        setRGB0(&s_sprites[i], 0x80, 0x80, 0x80);
        setXY0(&s_sprites[i], 170 + i * 32, 60);
        setWH(&s_sprites[i], 32, 110);
    }

    lom_draw_encyclopedia_picture(&s_sprites[0].r0, sizeof(SPRT), 3, s_page);
    gpu_draw(&s_sprites[0].r0, SPRITE_WORDS, sizeof(SPRT), 3);
}

/**
 * @brief Given the defaults, the encyclopedia's first page; when it draws the page's picture; then the Remaster's
 *        painting for the page is drawn once, where it was lined up with the PS1's, and the PS1's sprites draw nothing.
 */
SPECIFICATION(draws_the_painting, "Encyclopedia picture should draw the Remaster's painting for the page in place of the PS1's")
{
    when_the_encyclopedia_draws_a_pages_picture();

    CHECK(s_renderer_fake.picture_count == 1);
    CHECK(s_renderer_fake.pictures[0].colour == PAINTING);
    CHECK(memcmp(s_renderer_fake.pictures[0].xy, (int[8]){166, 54, 267, 54, 166, 180, 267, 180}, sizeof(int[8])) == 0);
    CHECK(s_sprites[1].code != 0x64 && s_sprites[2].code != 0x64);
}

/**
 * @brief Given a file that isn't one of the encyclopedia's pages the Remaster has a painting for; when the
 *        encyclopedia draws its picture; then the PS1's sprites are left as the game built them.
 */
SPECIFICATION(leaves_pages_without_a_painting, "Encyclopedia picture should leave the PS1's picture for a page the Remaster has no painting for")
{
    s_page = NOT_A_PAGE;

    when_the_encyclopedia_draws_a_pages_picture();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprites[0].code == 0x64);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the encyclopedia draws its first page's picture;
 *        then the PS1's sprites are left as the game built them.
 */
SPECIFICATION(leaves_the_ps1_picture_when_hd_screens_are_off, "Encyclopedia picture should leave the PS1's picture when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_encyclopedia_draws_a_pages_picture();

    CHECK(s_renderer_fake.picture_count == 0);
    CHECK(s_sprites[0].code == 0x64);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
