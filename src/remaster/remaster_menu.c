#include "lom_native_remaster.h"
#include "remaster_internal.h"
#include "remaster_text.h"
#include "lom_native_draw.h"
#include "lom_native_widescreen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MENU_BACKGROUND_FILE "system/image/mainmenu_bg.psb.m"
#define MENU_BACKGROUND "mainmenu_bg"
#define SPRITE_WORDS 4
#define ICON_W 34
#define ICON_H 16
#define PS1_W 320
#define PS1_H 224
#define HD_X 240
#define HD_Y 36
#define HD_W 1440
#define HD_H 1008
#define BAND 8
#define COLUMN_ROW 19
#define FIRST_COLUMN_X (-36)
#define SECOND_COLUMN_X 10
#define COLUMN_TOP 0
#define RETURN_X (-11)
#define RETURN_Y 203
#define KEY_FILE "system/image/keytop.psb.m"
#define KEY_FONT "system/font/debug_font.psb.m"
#define KEY_LETTERS_PER_100 42
#define KEY_END_W 26
#define KEY_H 76
#define PROMPT_BYTES 128

typedef struct
{
    int icon;
    const char* label;
    const char* mirror;
    int dx;
} MenuIcon;

static const MenuIcon s_menu_icons[] = {
    {1, "1p_male", "1p_male_stat", 0},
    {2, "1p_female", "1p_female_stat", 0},
    {3, "npc", "npc", 0},
    {4, "skill", "skill", 0},
    {5, "equip", "equip", 2},
    {6, "pet", "pet_stat", 0},
    {7, "pet_golem_stat", "pet_golem_stat", 0},
    {8, "golem", "golem", 0},
    {9, "golem_logic", "golem_logic", 0},
    {10, "item", "item", 0},
    {11, "weapon", "weapon_list", 0},
    {12, "armer", "armer_list", 0},
    {13, "instr", "instr_list", 3},
    {14, "artifact", "artifact", 3},
    {15, "weapon_stat", "weapon_stat", 0},
    {16, "armer_stat", "armer_stat", 0},
    {17, "instr_stat", "instr_stat", 0},
    {18, "weapon_arm_stat", "weapon_arm_stat", 0},
    {22, "return", "return", 0},
    {23, "npc", "npc", 0},
    {24, "diary", "event", 0},
    {110, "2p_male", "2p_male_stat", 0},
    {111, "2p_female", "2p_female_stat", 0},
};

#define MENU_ICON_COUNT ((int)(sizeof(s_menu_icons) / sizeof(s_menu_icons[0])))

static const char* const s_icon_files[LOM_REMASTER_TEXT_COUNT] = {"micon_en.psb.m", "micon_en.psb.m", "micon_de.psb.m", "micon_fr.psb.m", "micon_es.psb.m",
                                                                  "micon_jp.psb.m", "micon_en.psb.m", "micon_en.psb.m", "micon_en.psb.m"};

static int s_icons;
static int s_columns;

static const char* const s_prompt_keys[LOM_MENU_PROMPT_COUNT] = {"mcapt_67", "mcapt_68", "mcapt_69", "mcapt_70"};
static unsigned char s_prompts[LOM_MENU_PROMPT_COUNT][PROMPT_BYTES];
static int s_prompt_language = -1;
static int s_prompts_found;

/**
 * @brief The tab icons' file for the language the game's text is in, or the
 *        disc's: English for the languages the Remaster has none for.
 * @return The file.
 */
static const char* icon_file(void)
{
    const int text = lom_native_remaster_text_in_use();
#if defined(VERSION_JP)
    const int language = text > LOM_REMASTER_TEXT_PS1 && text < LOM_REMASTER_TEXT_COUNT ? text : LOM_REMASTER_TEXT_JAPANESE;
#else
    const int language = text > LOM_REMASTER_TEXT_PS1 && text < LOM_REMASTER_TEXT_COUNT ? text : LOM_REMASTER_TEXT_ENGLISH;
#endif
    return s_icon_files[language];
}

/**
 * @note The tab icons are the PS1's redrawn at 4.5 times with their names in
 *       each language, a little wider than the PS1's 32 by 16, from the same
 *       top left but for the few drawn further right. The second column's
 *       first row repeats the page its column is under, with a STATUS or
 *       LIST version of the icon where the Remaster has one.
 */
int lom_remaster_menu_icon_part(int icon, int mirror, const int rect[4], LomRemasterPart* part)
{
    for (int i = 0; i < MENU_ICON_COUNT; ++i)
    {
        const MenuIcon* entry = &s_menu_icons[i];

        if (entry->icon == icon)
        {
            memset(part, 0, sizeof(*part));
            snprintf(part->file, sizeof(part->file), "%s", icon_file());
            snprintf(part->label, sizeof(part->label), "%s", mirror ? entry->mirror : entry->label);
            const int x = rect[0] + entry->dx, y = rect[1];
            const int xy[8] = {x, y, x + ICON_W, y, x, y + ICON_H, x + ICON_W, y + ICON_H};
            memcpy(part->xy, xy, sizeof(xy));
            return 1;
        }
    }

    return 0;
}

/**
 * @note Where a tab goes in the Remaster's layout, on the PS1's picture: the
 *       first column, out in a wide frame's left side, has the characters and
 *       the item lists, the second the pages of the one open, 19 pixels a
 *       row as the PS1's, and Return has the box below them.
 */
void lom_remaster_menu_tab_xy(int column, int row, int* x, int* y)
{
    if (column >= 2)
    {
        *x = RETURN_X;
        *y = RETURN_Y;
        return;
    }

    *x = column ? SECOND_COLUMN_X : FIRST_COLUMN_X;
    *y = COLUMN_TOP + row * COLUMN_ROW;
}

/**
 * @brief Load the tab icons for the language in use.
 * @return Nonzero when every one can be drawn.
 */
static int icons_ready(void)
{
    for (int i = 0; i < MENU_ICON_COUNT; ++i)
    {
        if (!lom_remaster_picture_ready(icon_file(), s_menu_icons[i].label, NULL, NULL) ||
            !lom_remaster_picture_ready(icon_file(), s_menu_icons[i].mirror, NULL, NULL))
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Read the prompts' words from one of the Remaster's text files.
 * @param psb The file.
 * @return Nonzero when it has them all.
 */
static int read_prompts(const LomPsb* psb)
{
    size_t group;

    if (lom_psb_member(psb, psb->root, "menucapt", &group) != 0)
    {
        return 0;
    }

    for (int i = 0; i < LOM_MENU_PROMPT_COUNT; ++i)
    {
        char key[32];
        size_t value;
        const char* text;

        snprintf(key, sizeof(key), "%s:", s_prompt_keys[i]);

        if ((lom_psb_member(psb, group, key, &value) != 0 && lom_psb_member(psb, group, s_prompt_keys[i], &value) != 0) ||
            !(text = lom_psb_string(psb, value)) || remaster_text_convert(text, s_prompts[i], PROMPT_BYTES) < 0 || !s_prompts[i][0])
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Find the prompts' words in the language of the game's text: the
 *        Remaster's own text when it's in use, otherwise its English, which is
 *        the North American disc's language. The Japanese disc's text is stored
 *        differently, so there it has none.
 * @note They're only in the Remaster, after the strings the PS1 has, so the
 *       game's tables of text don't have them.
 */
static void find_prompts(void)
{
#if defined(VERSION_JP)
    s_prompts_found = 0;
#else
    const int text = lom_native_remaster_text_in_use();
    const int language = text > LOM_REMASTER_TEXT_PS1 ? text : LOM_REMASTER_TEXT_ENGLISH;

    if (language == s_prompt_language)
    {
        return;
    }

    s_prompt_language = language;
    memset(s_prompts, 0, sizeof(s_prompts));

    if (text > LOM_REMASTER_TEXT_PS1 && remaster_text_active() && remaster_text_language == text)
    {
        s_prompts_found = read_prompts(&remaster_text_psb);
        return;
    }

    char path[PATH_SIZE];
    size_t size = 0;
    LomPsb psb;
    snprintf(path, sizeof(path), "%s/text/%s.psb.m", lom_native_remaster_root(), remaster_text_languages[LOM_REMASTER_TEXT_ENGLISH]);
    unsigned char* data = lom_remaster_load(path, &size);
    s_prompts_found = data && lom_psb_open(&psb, data, size) == 0 && read_prompts(&psb);

    if (data)
    {
        lom_psb_close(&psb);
        free(data);
    }
#endif
}

int lom_remaster_menu_begin(void)
{
    s_icons = lom_native_remaster_found() && lom_native_remaster_uses(LOM_REMASTER_SWITCH_MENU) && icons_ready();
    s_columns = s_icons && lom_native_widescreen_enabled() && lom_remaster_picture_ready(MENU_BACKGROUND_FILE, MENU_BACKGROUND, NULL, NULL);

    if (s_columns && lom_remaster_picture_ready(KEY_FILE, "left", NULL, NULL) && lom_remaster_picture_ready(KEY_FILE, "mid", NULL, NULL) &&
        lom_remaster_picture_ready(KEY_FILE, "right", NULL, NULL))
    {
        find_prompts();
    }
    else
    {
        s_prompts_found = 0;
        s_prompt_language = -1;
    }

    return s_columns;
}

void lom_remaster_menu_end(void)
{
    s_icons = 0;
    s_columns = 0;
}

int lom_remaster_menu_columns(void)
{
    return s_columns;
}

void lom_remaster_menu_frame(void)
{
    lom_native_widescreen_begin_frame(s_columns);
}

void lom_remaster_menu_background(void* code, int stride, int count)
{
    if (!code || count <= 0 || !s_columns)
    {
        return;
    }

    const int margin = lom_native_widescreen_margin();
    const int u0 = HD_X - (margin * HD_W + PS1_W / 2) / PS1_W;
    const int u1 = HD_X + HD_W + (margin * HD_W + PS1_W / 2) / PS1_W;
    const int area[4] = {u0 < 0 ? 0 : u0, 0, (u1 > 1920 ? 1920 : u1) - (u0 < 0 ? 0 : u0), HD_H + 2 * HD_Y};
    const int xy[8] = {-margin, -BAND, PS1_W + margin, -BAND, -margin, PS1_H + BAND, PS1_W + margin, PS1_H + BAND};
    const int placement = lom_remaster_picture_place(MENU_BACKGROUND_FILE, MENU_BACKGROUND, area, xy, -1, -1);

    for (int i = 0; i < count; ++i)
    {
        lom_remaster_picture_command((unsigned char*)code + (size_t)i * (size_t)stride, SPRITE_WORDS, i == 0 ? placement : -1);
    }
}

void lom_remaster_menu_icon(void* code, int sprites, int stride, int icon, int mirror)
{
    if (!code || sprites <= 0 || !s_icons)
    {
        return;
    }

    for (int i = 0; i < sprites; ++i)
    {
        uint32_t* words = (uint32_t*)((unsigned char*)code + (size_t)i * (size_t)stride);
        const int rect[4] = {(int16_t)(words[1] & 0xFFFF), (int16_t)(words[1] >> 16), (int)(words[3] & 0xFFFF), (int)(words[3] >> 16)};
        LomRemasterPart part;

        if (!lom_remaster_menu_icon_part(icon, mirror, rect, &part))
        {
            return;
        }

        lom_remaster_picture_command(words, SPRITE_WORDS, lom_remaster_picture_place(part.file, part.label, NULL, part.xy, -1, -1));
    }
}

/**
 * @note The Remaster's key cap is a dark rounded button in three pictures:
 *       its ends keep their shape, scaled to the cap's height, and the middle
 *       stretches to fill the width. The button's name is written on it in a
 *       plain font, light grey, at high resolution, a little under half the
 *       cap's height.
 */
void lom_remaster_menu_key(void* code, int stride, int x, int y, int w, int h, const char* name)
{
    static const char* const parts[3] = {"left", "mid", "right"};

    if (!code || !s_prompts_found || w <= 0 || h <= 0 || !name)
    {
        return;
    }

    const int end = (h * KEY_END_W + KEY_H / 2) / KEY_H;
    const int edges[4] = {x, x + end, x + w - end, x + w};
    const int box_w = w * HD_W / PS1_W, box_h = h * HD_H / PS1_H;
    char label[48];
    snprintf(label, sizeof(label), "%s|%d|%d|%d", name, box_w, box_h, box_h * KEY_LETTERS_PER_100 / 100);
    const int whole[8] = {x, y, x + w, y, x, y + h, x + w, y + h};
    lom_remaster_picture_command(code, SPRITE_WORDS, lom_remaster_picture_place(KEY_FONT, label, NULL, whole, -1, -1));

    for (int i = 0; i < 3; ++i)
    {
        const int x0 = edges[i], x1 = edges[i + 1] > x0 ? edges[i + 1] : x0;
        const int xy[8] = {x0, y, x1, y, x0, y + h, x1, y + h};
        lom_remaster_picture_command((unsigned char*)code + (size_t)(i + 1) * (size_t)stride, SPRITE_WORDS,
                                     lom_remaster_picture_place(KEY_FILE, parts[i], NULL, xy, -1, -1));
    }
}

const unsigned char* lom_remaster_menu_prompt(int which)
{
    return s_prompts_found && which >= 0 && which < LOM_MENU_PROMPT_COUNT ? s_prompts[which] : NULL;
}
