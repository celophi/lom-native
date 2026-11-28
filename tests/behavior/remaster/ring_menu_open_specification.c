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

static int s_tab[3][2];

/**
 * @brief Open the ring menu, then ask where its tabs go, as MENU does as it lays the menu out.
 */
static void when_the_menu_opens(void)
{
    open_the_menu();
    lom_draw_menu_tab_xy(0, 2, &s_tab[0][0], &s_tab[0][1]);
    lom_draw_menu_tab_xy(1, 0, &s_tab[1][0], &s_tab[1][1]);
    lom_draw_menu_tab_xy(2, 0, &s_tab[2][0], &s_tab[2][1]);
}

/**
 * @brief Given the defaults, Widescreen and HD screens on; when the ring menu opens; then it uses the Remaster's icons
 *        laid out in the Remaster's two columns: the characters and lists out in the wide frame's left side, the open
 *        one's pages beside them, 19 pixels a row, and Return in the box below.
 */
SPECIFICATION(uses_the_remasters_columns, "Ring menu should use the Remaster's icons in its two columns when Widescreen and HD screens are on")
{
    when_the_menu_opens();

    CHECK(s_columns);
    CHECK(lom_draw_menu_hd());
    CHECK(lom_draw_menu_columns());
    CHECK(s_tab[0][0] == -36 && s_tab[0][1] == 38);
    CHECK(s_tab[1][0] == 10 && s_tab[1][1] == 0);
    CHECK(s_tab[2][0] == -11 && s_tab[2][1] == 203);
}

/**
 * @brief Given the player has Widescreen off, which the Remaster's columns need; when the ring menu opens; then it uses
 *        the Remaster's icons in the PS1's ring.
 */
SPECIFICATION(uses_the_remasters_icons_in_the_ring, "Ring menu should use the Remaster's icons in the PS1's ring when Widescreen is off")
{
    s_widescreen = 0;

    when_the_menu_opens();

    CHECK(!s_columns);
    CHECK(lom_draw_menu_hd());
    CHECK(!lom_draw_menu_columns());
}

/**
 * @brief Given the player turned the Remaster's HD screens off; when the ring menu opens; then it's the PS1's menu.
 */
SPECIFICATION(keeps_the_ps1_menu_when_hd_screens_are_off, "Ring menu should be the PS1's when HD screens are turned off")
{
    s_hd_screens = 0;

    when_the_menu_opens();

    CHECK(!lom_draw_menu_hd());
    CHECK(!lom_draw_menu_columns());
}

/**
 * @brief Given the Remaster's icons are missing the item list's; when the ring menu opens; then it keeps the PS1's
 *        icons rather than mixing the two.
 */
SPECIFICATION(keeps_the_ps1_icons_without_all_of_them, "Ring menu should keep the PS1's icons when the Remaster is missing any of them")
{
    s_icons[12].label = "not-an-icon";

    when_the_menu_opens();

    CHECK(!lom_draw_menu_hd());
}

/**
 * @brief Given the defaults; when the ring menu opens; then the words beside its LB and RB prompts are the Remaster's
 *        English ones, in the game's text.
 */
SPECIFICATION(has_the_remasters_prompt_words, "Ring menu should show the Remaster's words beside its LB and RB prompts")
{
    when_the_menu_opens();

    CHECK(strcmp((const char*)lom_draw_menu_prompt(LOM_MENU_PROMPT_STATUS_VIEW), "Status View") == 0);
    CHECK(strcmp((const char*)lom_draw_menu_prompt(LOM_MENU_PROMPT_WEAPON), "Weapons") == 0);
    CHECK(strcmp((const char*)lom_draw_menu_prompt(LOM_MENU_PROMPT_ARMOR), "Armour") == 0);
}

/**
 * @brief Given the Remaster's key caps are missing their middle; when the ring menu opens; then it has no words for
 *        its prompts, so the PS1's button icons are shown.
 */
SPECIFICATION(shows_the_ps1_prompts_without_key_caps, "Ring menu should show the PS1's prompts when the Remaster's key caps are missing")
{
    s_keys[1].label = "not-the-middle";

    when_the_menu_opens();

    CHECK(lom_draw_menu_prompt(LOM_MENU_PROMPT_WEAPON) == NULL);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
