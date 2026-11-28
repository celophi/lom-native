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
#define DATA REMASTER_FOLDER "/windata/"
#define SPRITE_WORDS 4
#define ITEM_ICON 0xA0A000FFu
#define EQUIP_ICON 0x00A0A0FFu
#define WEAPON_LIST_ICON 0xA000A0FFu
#define GERMAN_ICONS 0x406080FFu
#define MENU_BACKGROUND 0x202840FFu
#define KEY_LEFT 0x111111FFu
#define KEY_MIDDLE 0x222222FFu
#define KEY_RIGHT 0x333333FFu

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static RemasterPicture s_icons[] = {
    {"1p_male", 153, 72, 0x10000000u, 0, NULL, 0, NULL},
    {"1p_male_stat", 153, 72, 0x10010101u, 0, NULL, 0, NULL},
    {"1p_female", 153, 72, 0x10020202u, 0, NULL, 0, NULL},
    {"1p_female_stat", 153, 72, 0x10030303u, 0, NULL, 0, NULL},
    {"npc", 153, 72, 0x10040404u, 0, NULL, 0, NULL},
    {"skill", 153, 72, 0x10050505u, 0, NULL, 0, NULL},
    {"equip", 153, 72, EQUIP_ICON, 0, NULL, 0, NULL},
    {"pet", 153, 72, 0x10070707u, 0, NULL, 0, NULL},
    {"pet_stat", 153, 72, 0x10080808u, 0, NULL, 0, NULL},
    {"pet_golem_stat", 153, 72, 0x10090909u, 0, NULL, 0, NULL},
    {"golem", 153, 72, 0x100A0A0Au, 0, NULL, 0, NULL},
    {"golem_logic", 153, 72, 0x100B0B0Bu, 0, NULL, 0, NULL},
    {"item", 153, 72, ITEM_ICON, 0, NULL, 0, NULL},
    {"weapon", 153, 72, 0x100D0D0Du, 0, NULL, 0, NULL},
    {"weapon_list", 153, 72, WEAPON_LIST_ICON, 0, NULL, 0, NULL},
    {"armer", 153, 72, 0x100F0F0Fu, 0, NULL, 0, NULL},
    {"armer_list", 153, 72, 0x10101010u, 0, NULL, 0, NULL},
    {"instr", 153, 72, 0x10111111u, 0, NULL, 0, NULL},
    {"instr_list", 153, 72, 0x10121212u, 0, NULL, 0, NULL},
    {"artifact", 153, 72, 0x10131313u, 0, NULL, 0, NULL},
    {"weapon_stat", 153, 72, 0x10141414u, 0, NULL, 0, NULL},
    {"armer_stat", 153, 72, 0x10151515u, 0, NULL, 0, NULL},
    {"instr_stat", 153, 72, 0x10161616u, 0, NULL, 0, NULL},
    {"weapon_arm_stat", 153, 72, 0x10171717u, 0, NULL, 0, NULL},
    {"return", 153, 72, 0x10181818u, 0, NULL, 0, NULL},
    {"diary", 153, 72, 0x10191919u, 0, NULL, 0, NULL},
    {"event", 153, 72, 0x101A1A1Au, 0, NULL, 0, NULL},
    {"2p_male", 153, 72, 0x101B1B1Bu, 0, NULL, 0, NULL},
    {"2p_male_stat", 153, 72, 0x101C1C1Cu, 0, NULL, 0, NULL},
    {"2p_female", 153, 72, 0x101D1D1Du, 0, NULL, 0, NULL},
    {"2p_female_stat", 153, 72, 0x101E1E1Eu, 0, NULL, 0, NULL}
};
static RemasterPicture s_german_icons[] = {
    {"1p_male", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"1p_male_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"1p_female", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"1p_female_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"npc", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"skill", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"equip", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"pet", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"pet_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"pet_golem_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"golem", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"golem_logic", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"item", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"weapon", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"weapon_list", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"armer", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"armer_list", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"instr", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"instr_list", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"artifact", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"weapon_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"armer_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"instr_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"weapon_arm_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"return", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"diary", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"event", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"2p_male", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"2p_male_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"2p_female", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL},
    {"2p_female_stat", 153, 72, GERMAN_ICONS, 0, NULL, 0, NULL}
};
static RemasterPicture s_background[] = {{"mainmenu_bg", 1920, 1080, MENU_BACKGROUND, 0, NULL, 0, NULL}};
static RemasterPicture s_keys[] = {
    {"left", 26, 76, KEY_LEFT, 0, NULL, 0, NULL},
    {"mid", 40, 76, KEY_MIDDLE, 0, NULL, 0, NULL},
    {"right", 26, 76, KEY_RIGHT, 0, NULL, 0, NULL},
};
static int s_icon_count = 31;
static int s_hd_screens = 1;
static int s_widescreen = 1;
static int s_text = LOM_REMASTER_TEXT_ENGLISH;
static int s_columns;

/**
 * @brief Put in a fake of each provider the ring menu uses, with the happy path's defaults: the Remaster in Steam's
 *        library with the menu's icons, background, key caps and English words, and a renderer that can draw them.
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
 * @brief Give the fake the Remaster's English text, with the words the menu shows beside its LB and RB prompts.
 */
static void the_remaster_has_its_english_words(void)
{
    static RemasterPsbBuilder s_builder;
    size_t size = 0;
    remaster_psb_start(&s_builder);
    RemasterPsbNode* root = remaster_psb_object(&s_builder);
    RemasterPsbNode* words = remaster_psb_add(root, "menucapt", remaster_psb_object(&s_builder));
    remaster_psb_add(words, "mcapt_67", remaster_psb_text(&s_builder, "Status View"));
    remaster_psb_add(words, "mcapt_68", remaster_psb_text(&s_builder, "Weapons"));
    remaster_psb_add(words, "mcapt_69", remaster_psb_text(&s_builder, "Armour"));
    remaster_psb_add(words, "mcapt_70", remaster_psb_text(&s_builder, "Music"));
    unsigned char* file = remaster_psb_build(&s_builder, root, &size);
    remaster_files_provider_fake_add(&s_files_fake, DATA "text/EN_US.psb.m", file, size);
}

/**
 * @brief Start the game with the launcher's settings and the player's Widescreen setting, then open the ring menu,
 *        which is when it decides how it's laid out.
 */
static void open_the_menu(void)
{
    char hd_screens[8], text[8];
    the_remaster_has(DATA "micon_en.psb.m", s_icons, s_icon_count);
    the_remaster_has(DATA "micon_de.psb.m", s_german_icons, s_icon_count);
    the_remaster_has(DATA "system/image/mainmenu_bg.psb.m", s_background, 1);
    the_remaster_has(DATA "system/image/keytop.psb.m", s_keys, 3);
    the_remaster_has_its_english_words();
    snprintf(hd_screens, sizeof(hd_screens), "%d", s_hd_screens);
    snprintf(text, sizeof(text), "%d", s_text);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_UI, hd_screens, 1);
    setenv(LOM_ENV_REMASTER_TEXT, text, 1);
    lom_native_remaster_initialize();
    lom_native_widescreen_set_enabled(s_widescreen);
    lom_draw_menu_begin();
    s_columns = lom_draw_menu_columns();
}
#define ICON_ITEM 10
#define ICON_EQUIP 5
#define ICON_WEAPON 11

static int s_icon = ICON_ITEM;
static int s_mirror = 0;
static SPRT s_sprite;

/**
 * @brief Open the ring menu, then have it build a tab's icon as a sprite of the PS1's, hand it to the port and draw it,
 *        as MENU does each frame.
 */
static void when_the_menu_draws_an_icon(void)
{
    open_the_menu();
    setSprt(&s_sprite);
    setRGB0(&s_sprite, 0x80, 0x80, 0x80);
    setXY0(&s_sprite, 40, 60);
    setWH(&s_sprite, 32, 16);
    lom_draw_menu_icon(&s_sprite.r0, 1, sizeof(SPRT), s_icon, s_mirror);
    gpu_draw(&s_sprite.r0, SPRITE_WORDS, sizeof(SPRT), 1);
}

/**
 * @brief Given the defaults, the item tab; when the ring menu draws its icon; then the Remaster's item icon is drawn
 *        from the PS1's icon's top left, a little wider than the PS1's.
 */
SPECIFICATION(draws_the_icon, "Ring menu icon should draw the Remaster's icon from the PS1's icon's top left")
{
    when_the_menu_draws_an_icon();

    CHECK(s_renderer_fake.quad_count == 1);
    CHECK(s_renderer_fake.quads[0].colour == ITEM_ICON);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){40, 60, 74, 60, 40, 76, 74, 76}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the equipment tab, whose Remaster icon sits further right; when the ring menu draws it; then it's drawn
 *        2 pixels right of the PS1's.
 */
SPECIFICATION(draws_some_icons_further_right, "Ring menu icon should draw the equipment icon a little right of the PS1's")
{
    s_icon = ICON_EQUIP;

    when_the_menu_draws_an_icon();

    CHECK(s_renderer_fake.quads[0].colour == EQUIP_ICON);
    CHECK(memcmp(s_renderer_fake.quads[0].xy, (int[8]){42, 60, 76, 60, 42, 76, 76, 76}, sizeof(int[8])) == 0);
}

/**
 * @brief Given the weapon tab repeated at the top of the second column; when the ring menu draws it; then the
 *        Remaster's LIST version of the icon is drawn.
 */
SPECIFICATION(draws_the_list_icon_in_the_second_column, "Ring menu icon should draw the Remaster's list icon when it heads the second column")
{
    s_icon = ICON_WEAPON;
    s_mirror = 1;

    when_the_menu_draws_an_icon();

    CHECK(s_renderer_fake.quads[0].colour == WEAPON_LIST_ICON);
}

/**
 * @brief Given the Remaster's text is in German; when the ring menu draws a tab's icon; then the Remaster's German
 *        icon is drawn, with its German name.
 */
SPECIFICATION(draws_the_german_icon, "Ring menu icon should draw the German icon when the Remaster's text is in German")
{
    s_text = LOM_REMASTER_TEXT_GERMAN;

    when_the_menu_draws_an_icon();

    CHECK(s_renderer_fake.quads[0].colour == GERMAN_ICONS);
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the ring menu draws a tab's icon; then the PS1's
 *        sprite is left as the game built it.
 */
SPECIFICATION(leaves_the_ps1_icon_when_hd_screens_are_off, "Ring menu icon should leave the PS1's icon when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_menu_draws_an_icon();

    CHECK(s_renderer_fake.quad_count == 0);
    CHECK(s_sprite.code == 0x64);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
