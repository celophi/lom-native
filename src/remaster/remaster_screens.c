#include "lom_native_remaster.h"
#include "remaster_internal.h"
#include "lom_native_widescreen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define WSEL_FILE "map_select.psb.m"

enum
{
    WSEL_LAND_MAP = 0,
    WSEL_WORLD_MAP = 1,
    WSEL_CURSOR = 2
};

typedef struct
{
    int picture;
    const char* label;
    int ps1[4];
    int hd[4];
    int wide;
} WselArt;

static const WselArt s_wsel_art[] = {
    {WSEL_LAND_MAP, "worldmap_map", {0, 0, 416, 416}, {0, 0, 1920, 1920}, 0},
    {WSEL_WORLD_MAP, "map_select_bg", {0, 0, 320, 224}, {240, 36, 1440, 1008}, 1},
    {WSEL_WORLD_MAP, "map_select_map", {96, 48, 128, 120}, {5, 5, 563, 527}, 0},
    {WSEL_CURSOR, "worldmap_frame", {0, 0, 120, 118}, {0, 0, 500, 500}, 0},
};

#define WSEL_ART_COUNT ((int)(sizeof(s_wsel_art) / sizeof(s_wsel_art[0])))
#define WSEL_ZOOM_ART 2
#define SPRITE_WORDS 4
#define QUAD_WORDS 9

/**
 * @brief Check whether the Remaster's pictures are drawn.
 * @return Nonzero when the Remaster was found and HD pictures are on.
 */
static int enabled(void)
{
    return lom_native_remaster_found() && lom_native_remaster_hd_ui();
}

/**
 * @brief Load all the Remaster's art for one of WSEL's pictures.
 * @param picture Which picture.
 * @return Nonzero when it has art and all of it can be drawn, so a picture is
 *         never half the PS1's and half the Remaster's.
 */
static int wsel_art_ready(int picture)
{
    int found = 0;

    for (int i = 0; i < WSEL_ART_COUNT; ++i)
    {
        if (s_wsel_art[i].picture == picture)
        {
            if (!lom_remaster_picture_ready(WSEL_FILE, s_wsel_art[i].label, NULL, NULL))
            {
                return 0;
            }

            found = 1;
        }
    }

    return found;
}

/**
 * @brief Scale a distance from one size to another, rounding.
 * @param value The distance.
 * @param from The size it's measured against.
 * @param to The size to scale it to.
 * @return The scaled distance.
 */
static int scale(int value, int from, int to)
{
    return (value * to + from / 2) / from;
}

/**
 * @brief Make the placements that draw the Remaster's art for one sprite of
 *        one of WSEL's pictures: every part of the art the sprite covers, in
 *        the table's order.
 * @note s_wsel_art says where each repainted picture goes, as x, y, width and
 *       height on the PS1's picture and on the Remaster's, found by lining the
 *       two up. The wall is the PS1's at 4.5 times, with more of the room at
 *       the sides. The small map on it is its own picture, drawn over the
 *       wall's, and it's also what the zoom stretches; it has a black edge the
 *       PS1's doesn't, which is left out. The PS1 draws the hero, the prompt,
 *       the darkening and the cell shading, which the Remaster has no art for.
 *       The wall is painted wider than the PS1's, so in a wide frame its
 *       strips at the picture's sides reach on into the frame's sides.
 * @param picture Which picture.
 * @param x Where the picture's top left is on the screen.
 * @param y The same, down.
 * @param rect The sprite: x, y, width and height on the screen.
 * @return The first placement, or -1 when there's nothing to draw.
 */
static int wsel_place(int picture, int x, int y, const int rect[4])
{
    int first = -1, last = -1;

    for (int i = 0; i < WSEL_ART_COUNT; ++i)
    {
        const WselArt* art = &s_wsel_art[i];

        if (art->picture != picture)
        {
            continue;
        }

        const int left = rect[0] - x, top = rect[1] - y;
        const int x0 = left > art->ps1[0] ? left : art->ps1[0];
        const int y0 = top > art->ps1[1] ? top : art->ps1[1];
        const int x1 = left + rect[2] < art->ps1[0] + art->ps1[2] ? left + rect[2] : art->ps1[0] + art->ps1[2];
        const int y1 = top + rect[3] < art->ps1[1] + art->ps1[3] ? top + rect[3] : art->ps1[1] + art->ps1[3];

        if (x0 >= x1 || y0 >= y1)
        {
            continue;
        }

        const int margin = art->wide ? lom_native_widescreen_margin() : 0;
        const int wide_x0 = x0 == art->ps1[0] ? x0 - margin : x0;
        const int wide_x1 = x1 == art->ps1[0] + art->ps1[2] ? x1 + margin : x1;
        const int u0 = art->hd[0] + scale(wide_x0 - art->ps1[0], art->ps1[2], art->hd[2]);
        const int v0 = art->hd[1] + scale(y0 - art->ps1[1], art->ps1[3], art->hd[3]);
        const int u1 = art->hd[0] + scale(wide_x1 - art->ps1[0], art->ps1[2], art->hd[2]);
        const int v1 = art->hd[1] + scale(y1 - art->ps1[1], art->ps1[3], art->hd[3]);
        const int part[4] = {u0, v0, u1 - u0, v1 - v0};
        const int xy[8] = {x + wide_x0, y + y0, x + wide_x1, y + y0, x + wide_x0, y + y1, x + wide_x1, y + y1};
        const int placed = lom_remaster_picture_place(WSEL_FILE, art->label, part, xy, -1, last);

        if (placed < 0)
        {
            return -1;
        }

        last = placed;
        first = first < 0 ? placed : first;
    }

    return first;
}

void lom_remaster_wsel_loaded(void)
{
    if (enabled())
    {
        for (int picture = WSEL_LAND_MAP; picture <= WSEL_CURSOR; ++picture)
        {
            wsel_art_ready(picture);
        }
    }
}

/**
 * @brief Draw the Remaster's art for one of WSEL's pictures over the sprites
 *        the game built for it. The cursor's drop shadow draws nothing, since
 *        the Remaster's frame has its own.
 * @param code The first sprite's code word.
 * @param stride How far each next sprite's is, in bytes.
 * @param count How many sprites.
 * @param picture Which picture (WSEL_SPRITE_*).
 * @param shadow Nonzero for the cursor's drop shadow.
 * @param x Where its top left is on the screen.
 * @param y The same, down.
 */
void lom_remaster_wsel_picture(void* code, int stride, int count, int picture, int shadow, int x, int y)
{
    if (!code || count <= 0 || !enabled() || !wsel_art_ready(picture))
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        uint32_t* words = (uint32_t*)((unsigned char*)code + (size_t)i * (size_t)stride);

        if (shadow)
        {
            lom_remaster_picture_command(words, SPRITE_WORDS, -1);
            continue;
        }

        const int rect[4] = {(int16_t)(words[1] & 0xFFFF), (int16_t)(words[1] >> 16), (int)(words[3] & 0xFFFF), (int)(words[3] >> 16)};
        const int placement = wsel_place(picture, x, y, rect);

        if (placement >= 0)
        {
            lom_remaster_picture_command(words, SPRITE_WORDS, placement);
        }
    }
}

int lom_remaster_wsel_wide(void)
{
    return enabled() && wsel_art_ready(WSEL_WORLD_MAP);
}

void lom_remaster_wsel_zoom(void* code)
{
    if (!code || !enabled() || !wsel_art_ready(WSEL_WORLD_MAP))
    {
        return;
    }

    uint32_t* words = code;
    int xy[8];

    for (int corner = 0; corner < 4; ++corner)
    {
        const uint32_t point = words[1 + corner * 2];
        xy[corner * 2] = (int16_t)(point & 0xFFFF);
        xy[corner * 2 + 1] = (int16_t)(point >> 16);
    }

    const WselArt* art = &s_wsel_art[WSEL_ZOOM_ART];
    const int placement = lom_remaster_picture_place(WSEL_FILE, art->label, art->hd, xy, (int)(words[4] >> 16), -1);

    if (placement >= 0)
    {
        lom_remaster_picture_command(words, QUAD_WORDS, placement);
    }
}

#define KAMEO_FILE "kameo.psb.m"
#define CELL_W 48
#define CELL_H 56
#define KAMEO_W 260
#define KAMEO_H 300
#define DAYS 6
#define SHELL_ROW 3
#define DAY_NAME_SCALE_NUM 9
#define DAY_NAME_SCALE_DEN 2
#define DAY_NAME_DROP 4
#define HIDUKE_W 512
#define HIDUKE_H 1024
#define PANEL_FILE "mana_panel.psb.m"
#define PANEL_STAGES 10
#define LABEL_SCALE_NUM 2
#define LABEL_SCALE_DEN 9
#define LAND_CLEARED_CLUT_ROW 506
#define LANDS 64

static const char* const s_day_spirits[DAYS] = {"salamander_k", "aura_k", "gnome_k", "jinn_k", "dryad_k", "undine_k"};

static int s_status_last = -1;

static const char* const s_day_name_files[LOM_REMASTER_TEXT_COUNT] = {"hiduke_en", "hiduke_en", "hiduke_ge", "hiduke_fr", "hiduke_sp",
                                                                      "hiduke",    "hiduke_kr", "hiduke_cn", "hiduke_cn"};
static const char* const s_land_name_folders[LOM_REMASTER_TEXT_COUNT] = {"US", "US", "DE", "FR", "ES", "JP", "KR", "CN", "TW"};

/**
 * @brief The language the world map's names are in: the text the scenes use,
 *        or the disc's own.
 * @return A LOM_REMASTER_TEXT_* other than LOM_REMASTER_TEXT_PS1.
 */
static int names_language(void)
{
    const int text = lom_native_remaster_text_in_use();

    if (text > LOM_REMASTER_TEXT_PS1 && text < LOM_REMASTER_TEXT_COUNT)
    {
        return text;
    }

#if defined(VERSION_JP)
    return LOM_REMASTER_TEXT_JAPANESE;
#else
    return LOM_REMASTER_TEXT_ENGLISH;
#endif
}

/**
 * @brief Read a sprite's screen rectangle and texture position.
 * @param words The sprite's words from its code word.
 * @param rect Set to x, y, width and height on the screen.
 * @param u Set to its texture position across.
 * @param v And down.
 */
static void read_sprite(const uint32_t* words, int rect[4], int* u, int* v)
{
    rect[0] = (int16_t)(words[1] & 0xFFFF);
    rect[1] = (int16_t)(words[1] >> 16);
    rect[2] = (int)(words[3] & 0xFFFF);
    rect[3] = (int)(words[3] >> 16);
    *u = (int)(words[2] & 0xFF);
    *v = (int)((words[2] >> 8) & 0xFF);
}

/**
 * @brief Turn a sprite into a draw of part of one of the Remaster's
 *        pictures, over the same rectangle.
 * @param words The sprite's words from its code word.
 * @param file The picture's file.
 * @param rect The sprite's screen rectangle.
 * @param part The part of the picture.
 */
static void replace_sprite(uint32_t* words, const char* file, const int rect[4], const int part[4])
{
    const int xy[8] = {rect[0], rect[1], rect[0] + rect[2], rect[1], rect[0], rect[1] + rect[3], rect[0] + rect[2], rect[1] + rect[3]};
    const int placement = lom_remaster_picture_place(file, NULL, part, xy, -1, -1);

    if (placement >= 0)
    {
        lom_remaster_picture_command(words, SPRITE_WORDS, placement);
    }
}

/**
 * @brief Draw the Remaster's art for a part of the world map's status corner.
 * @param code The sprite's code word.
 * @param part LOM_REMASTER_WMAP_*.
 * @note The PS1 keeps the six days' spirits in their shells as 48 by 56
 *       cells, two to a row, with the empty shell on the fourth row; each of
 *       the Remaster's spirits and its shell is one such cell at about 5.4
 *       times. The day names are one 96 by 192 picture, a 32-pixel row a day,
 *       and the Remaster's are the same at 4.5 times, at the left of a wider
 *       picture, in each language, but each name sits 4 pixels higher in its
 *       row than the PS1's, so it's drawn that much lower to stay on the glow
 *       behind it. The
 *       PS1 draws the spirit, then its shell around it, through the hole in
 *       the shell. The Remaster's shell is solid inside, with the spirit on
 *       top, so the spirits are drawn after the shell, by the shell's command
 *       (s_status_last is this frame's last placement in it), each in its own
 *       color and blending for its fade.
 */
void lom_remaster_wmap_status(void* code, int part)
{
    if (!code || !enabled())
    {
        return;
    }

    uint32_t* words = code;
    int rect[4], u, v;
    read_sprite(words, rect, &u, &v);

    if (part == LOM_REMASTER_WMAP_DAY_NAME)
    {
        char file[64];
        snprintf(file, sizeof(file), "system/image/%s.psb.m", s_day_name_files[names_language()]);
        const int area[4] = {scale(u, DAY_NAME_SCALE_DEN, DAY_NAME_SCALE_NUM), scale(v, DAY_NAME_SCALE_DEN, DAY_NAME_SCALE_NUM),
                             scale(rect[2], DAY_NAME_SCALE_DEN, DAY_NAME_SCALE_NUM), scale(rect[3], DAY_NAME_SCALE_DEN, DAY_NAME_SCALE_NUM)};
        const int lowered[4] = {rect[0], rect[1] + DAY_NAME_DROP, rect[2], rect[3]};

        if (area[0] + area[2] <= HIDUKE_W && area[1] + area[3] <= HIDUKE_H)
        {
            replace_sprite(words, file, lowered, area);
        }

        return;
    }

    const int column = u / CELL_W, row = v / CELL_H, day = row * 2 + column;
    const int shell = part == LOM_REMASTER_WMAP_SHELL;
    const char* label = shell ? (row == SHELL_ROW && column == 0 ? "none_k" : NULL) : row < SHELL_ROW && day < DAYS ? s_day_spirits[day] : NULL;
    int w = 0, h = 0;

    if (shell)
    {
        s_status_last = -1;
    }

    if (!label || !lom_remaster_picture_ready(KAMEO_FILE, label, &w, &h) || w != KAMEO_W || h != KAMEO_H)
    {
        return;
    }

    const int area[4] = {scale(u - column * CELL_W, CELL_W, KAMEO_W), scale(v - row * CELL_H, CELL_H, KAMEO_H), scale(rect[2], CELL_W, KAMEO_W),
                         scale(rect[3], CELL_H, KAMEO_H)};

    if (area[0] + area[2] > KAMEO_W || area[1] + area[3] > KAMEO_H)
    {
        return;
    }

    const int xy[8] = {rect[0], rect[1], rect[0] + rect[2], rect[1], rect[0], rect[1] + rect[3], rect[0] + rect[2], rect[1] + rect[3]};

    if (shell)
    {
        const int placement = lom_remaster_picture_place(KAMEO_FILE, label, area, xy, -1, -1);

        if (placement >= 0)
        {
            lom_remaster_picture_command(words, SPRITE_WORDS, placement);
            s_status_last = placement;
        }

        return;
    }

    if (s_status_last < 0)
    {
        return;
    }

    const unsigned flags = words[0] >> 24;
    const int placement = lom_remaster_picture_place(KAMEO_FILE, label, area, xy, -1, s_status_last);

    if (placement >= 0)
    {
        lom_remaster_picture_style(placement, flags & 1 ? 0x808080 : (int)(words[0] & 0xFFFFFF), (flags & 2) != 0);
        lom_remaster_picture_command(words, SPRITE_WORDS, -1);
        s_status_last = placement;
    }
}

/**
 * @brief Draw the Remaster's banner for a land's name on the world map.
 * @param code The sprite's code word.
 * @param selection The land, or 64 plus it for its artifact's name.
 * @param tpage The name's texture page.
 * @note A land's name glows green once it's cleared and yellow before, as the
 *       Remaster's two banners do; an artifact's name has its own. The banner
 *       is centered on the PS1's name, at the Remaster's 4.5 times. The PS1
 *       adds its name to what's under it, which would wash out the banner's
 *       colors, so the banner is drawn over it instead.
 */
void lom_remaster_wmap_label(void* code, int selection, int tpage)
{
    if (!code || selection < 0 || selection >= 2 * LANDS || !enabled())
    {
        return;
    }

    uint32_t* words = code;
    int rect[4], u, v;
    read_sprite(words, rect, &u, &v);

    const int land = selection % LANDS, clut_row = (int)((words[2] >> 22) & 0x1FF);
    const char* kind = selection >= LANDS ? "artifactname" : (clut_row & ~1) == LAND_CLEARED_CLUT_ROW ? "landname" : "landname_c";
    char file[96];
    int w = 0, h = 0;
    snprintf(file, sizeof(file), "system/image/land_name/%s/map_%s_%04d.psb.m", s_land_name_folders[names_language()], kind, land + 1);

    if (!lom_remaster_picture_ready(file, NULL, &w, &h))
    {
        return;
    }

    int bounds[4] = {0, 0, rect[2], rect[3]};
    lom_remaster_vram_bounds((tpage & 0xF) * 64, (tpage >> 4 & 1) * 256, u, v, rect[2], rect[3], bounds);
    const int center_x2 = 2 * (rect[0] + bounds[0]) + bounds[2], center_y2 = 2 * (rect[1] + bounds[1]) + bounds[3];
    const int half_w2 = w * LABEL_SCALE_NUM / LABEL_SCALE_DEN, half_h2 = h * LABEL_SCALE_NUM / LABEL_SCALE_DEN;
    const int x0 = (center_x2 - half_w2) / 2, y0 = (center_y2 - half_h2) / 2, x1 = (center_x2 + half_w2 + 1) / 2, y1 = (center_y2 + half_h2 + 1) / 2;
    const int xy[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    const int placement = lom_remaster_picture_place(file, NULL, NULL, xy, -1, -1);

    if (placement >= 0)
    {
        lom_remaster_picture_style(placement, -1, 0);
        lom_remaster_picture_command(words, SPRITE_WORDS, placement);
    }
}

/**
 * @brief Draw the Remaster's panel in place of the world map's. It's the
 *        PS1's at 4.5 times, so it covers the same four points.
 * @param code The quad's code word.
 * @param stage The tree's growth, from 0.
 */
void lom_remaster_wmap_panel(void* code, int stage)
{
    if (!code || !enabled())
    {
        return;
    }

    char label[16];
    snprintf(label, sizeof(label), "panel%02d", (stage < 0 ? 0 : stage >= PANEL_STAGES ? PANEL_STAGES - 1 : stage) + 1);

    uint32_t* words = code;
    int xy[8];

    for (int corner = 0; corner < 4; ++corner)
    {
        const uint32_t point = words[1 + corner * 2];
        xy[corner * 2] = (int16_t)(point & 0xFFFF);
        xy[corner * 2 + 1] = (int16_t)(point >> 16);
    }

    const int placement = lom_remaster_picture_place(PANEL_FILE, label, NULL, xy, (int)(words[4] >> 16), -1);

    if (placement >= 0)
    {
        lom_remaster_picture_command(words, QUAD_WORDS, placement);
    }
}

#define SPIRIT_FILE "spirit_anime.psb.m"
#define METER_FILE "spirit_meter.psb.m"
#define SPIRITS 8
#define SPIRIT_FRAME 24
#define ORB 16
#define ORB_U 128
#define ORB_ROWS 4
#define ORB_SPRITES 16

static const char* const s_spirit_names[SPIRITS] = {"wilowisp", "sheid", "dryad", "aura", "salamander", "gnome", "jinn", "undine"};
static const int s_spirit_ps1_frames[SPIRITS] = {4, 4, 4, 4, 4, 5, 4, 3};
static const int s_spirit_hd_frames[SPIRITS] = {4, 3, 4, 4, 4, 6, 5, 4};

static const int s_orb_fill[ORB_SPRITES] = {0, -1, -1, -1, 0, 1, -1, -1, 0, -1, 2, -1, 0, -1, -1, 3};
static const int s_orb_shade[ORB_SPRITES] = {0x80, 0, 0, 0, 0x60, 0x80, 0, 0, 0x40, 0, 0x80, 0, 0x20, 0, 0, 0x80};

static const int s_orb_art[4] = {30, 30, 68, 68};
static const int s_orb_area[4] = {2, 2, 13, 12};
static const int s_spirit_area[4] = {2, 2, 20, 20};

/**
 * @brief Draw the Remaster's art for a world map spirit or its strength.
 * @param code The sprite's code word.
 * @param spirit Which spirit, in the game's order.
 * @param kind LOM_REMASTER_WMAP_SPIRIT_*.
 * @note The PS1 animates each spirit in its own row of 24 by 24 frames; the
 *       Remaster has its own frames, not always as many, so the PS1's are
 *       spread across them. A spirit's strength in a place is one of 16
 *       16-by-16 orbs, but the game only uses seven: empty for neutral, a
 *       third, two thirds or all of it in the spirit's color for stronger, and
 *       darker empty ones for weaker. The Remaster has no art for those, so
 *       its empty orb is darkened the same way (s_orb_fill and s_orb_shade,
 *       -1 for the orbs the game doesn't use). s_orb_art is where the
 *       Remaster's orb is in its picture; s_orb_area and s_spirit_area are
 *       where the PS1's orb and spirit are in their sprites. The PS1 marks
 *       these sprites semi-transparent as they fade, but their texels aren't,
 *       so they stay solid and only darken, never past the art as it's
 *       painted. The Remaster names Salamander's two fuller orbs without the
 *       m.
 */
void lom_remaster_wmap_spirit(void* code, int spirit, int kind)
{
    if (!code || spirit < 0 || spirit >= SPIRITS || !enabled())
    {
        return;
    }

    uint32_t* words = code;
    int rect[4], u, v;
    char label[32];
    const char* file;
    int shade = 0x80;
    read_sprite(words, rect, &u, &v);

    if (kind == LOM_REMASTER_WMAP_SPIRIT_ICON)
    {
        const int frame = u / SPIRIT_FRAME;

        if (rect[2] != SPIRIT_FRAME || rect[3] != SPIRIT_FRAME || v / SPIRIT_FRAME != spirit || frame >= s_spirit_ps1_frames[spirit])
        {
            return;
        }

        file = SPIRIT_FILE;
        snprintf(label, sizeof(label), "%s%d", s_spirit_names[spirit], frame * s_spirit_hd_frames[spirit] / s_spirit_ps1_frames[spirit] + 1);
    }
    else
    {
        const int sprite = (u - ORB_U) / ORB * ORB_ROWS + v / ORB;

        if (rect[2] != ORB || rect[3] != ORB || u < ORB_U || sprite < 0 || sprite >= ORB_SPRITES || s_orb_fill[sprite] < 0)
        {
            return;
        }

        const int fill = s_orb_fill[sprite];
        file = METER_FILE;
        shade = s_orb_shade[sprite];

        if (fill == 0)
        {
            snprintf(label, sizeof(label), "zero");
        }
        else
        {
            snprintf(label, sizeof(label), "%s%d%s", s_spirit_names[spirit], fill, spirit == 4 && fill > 1 ? "" : "m");
        }
    }

    const int* area = kind == LOM_REMASTER_WMAP_SPIRIT_ICON ? s_spirit_area : s_orb_area;
    const int x0 = rect[0] + area[0], y0 = rect[1] + area[1], x1 = x0 + area[2], y1 = y0 + area[3];
    const int xy[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    const int placement = lom_remaster_picture_place(file, label, kind == LOM_REMASTER_WMAP_SPIRIT_ICON ? NULL : s_orb_art, xy, -1, -1);

    if (placement < 0)
    {
        return;
    }

    const unsigned flags = words[0] >> 24;
    const unsigned color = flags & 1 ? 0x808080 : words[0] & 0xFFFFFF;
    unsigned shaded = 0;

    for (int channel = 0; channel < 24; channel += 8)
    {
        const unsigned level = (color >> channel) & 0xFF;
        shaded |= ((level < 0x80 ? level : 0x80) * (unsigned)shade / 0x80) << channel;
    }

    lom_remaster_picture_style(placement, (int)shaded, 0);
    lom_remaster_picture_command(words, SPRITE_WORDS, placement);
}
