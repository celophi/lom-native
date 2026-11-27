#include "lom_native_remaster.h"
#include "remaster_internal.h"
#include "lom_native_widescreen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
 * @brief Make a part that covers a rectangle on the screen.
 * @param part The part, whose xy is set.
 * @param x0 The rectangle's left.
 * @param y0 Its top.
 * @param x1 Its right.
 * @param y1 Its bottom.
 */
static void set_rect_xy(LomRemasterPart* part, int x0, int y0, int x1, int y1)
{
    const int xy[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    memcpy(part->xy, xy, sizeof(xy));
}

/**
 * @brief Make the part of a repainted picture that one rectangle of the PS1's
 *        covers, scaled from where the PS1's picture is to where the
 *        Remaster's is. A picture painted wide reaches on into a wide frame's
 *        sides from the PS1 picture's edges.
 * @param file The Remaster's file.
 * @param label The picture's label in it.
 * @param ps1 Where the PS1's picture is: x, y, width and height.
 * @param hd Where the same is on the Remaster's picture.
 * @param wide Nonzero when the Remaster's picture is painted wide.
 * @param x Where the PS1's picture's top left is on the screen.
 * @param y The same, down.
 * @param rect The rectangle: x, y, width and height on the screen.
 * @param margin The wide frame's margin, or 0.
 * @param part Set to the part.
 * @return 1 when the rectangle covers some of the picture, otherwise 0.
 */
static int art_part(const char* file, const char* label, const int ps1[4], const int hd[4], int wide, int x, int y, const int rect[4], int margin,
                    LomRemasterPart* part)
{
    const int left = rect[0] - x, top = rect[1] - y;
    const int x0 = left > ps1[0] ? left : ps1[0];
    const int y0 = top > ps1[1] ? top : ps1[1];
    const int x1 = left + rect[2] < ps1[0] + ps1[2] ? left + rect[2] : ps1[0] + ps1[2];
    const int y1 = top + rect[3] < ps1[1] + ps1[3] ? top + rect[3] : ps1[1] + ps1[3];

    if (x0 >= x1 || y0 >= y1)
    {
        return 0;
    }

    const int reach = wide ? margin : 0;
    const int wide_x0 = x0 == ps1[0] ? x0 - reach : x0;
    const int wide_x1 = x1 == ps1[0] + ps1[2] ? x1 + reach : x1;
    const int u0 = hd[0] + scale(wide_x0 - ps1[0], ps1[2], hd[2]);
    const int v0 = hd[1] + scale(y0 - ps1[1], ps1[3], hd[3]);
    const int u1 = hd[0] + scale(wide_x1 - ps1[0], ps1[2], hd[2]);
    const int v1 = hd[1] + scale(y1 - ps1[1], ps1[3], hd[3]);
    memset(part, 0, sizeof(*part));
    snprintf(part->file, sizeof(part->file), "%s", file);
    snprintf(part->label, sizeof(part->label), "%s", label);
    part->area[0] = u0;
    part->area[1] = v0;
    part->area[2] = u1 - u0;
    part->area[3] = v1 - v0;
    set_rect_xy(part, x + wide_x0, y + y0, x + wide_x1, y + y1);
    return 1;
}

/**
 * @brief Make the placements that draw parts, in order, chained into one
 *        command.
 * @param parts The parts.
 * @param count How many.
 * @return The first placement, or -1 when there's nothing to draw or one of
 *         them can't be.
 */
static int place_parts(const LomRemasterPart parts[], int count)
{
    int first = -1, last = -1;

    for (int i = 0; i < count; ++i)
    {
        const int placed = lom_remaster_picture_place(parts[i].file, parts[i].label, parts[i].area, parts[i].xy, -1, last);

        if (placed < 0)
        {
            return -1;
        }

        last = placed;
        first = first < 0 ? placed : first;
    }

    return first;
}

/**
 * @note s_wsel_art says where each repainted picture goes, as x, y, width and
 *       height on the PS1's picture and on the Remaster's, found by lining the
 *       two up. The wall is the PS1's at 4.5 times, with more of the room at
 *       the sides. The small map on it is its own picture, drawn over the
 *       wall's, and it's also what the zoom stretches; it has a black edge the
 *       PS1's doesn't, which is left out. The PS1 draws the hero, the prompt,
 *       the darkening and the cell shading, which the Remaster has no art for.
 *       The wall is painted wider than the PS1's, so in a wide frame its
 *       strips at the picture's sides reach on into the frame's sides.
 */
int lom_remaster_wsel_parts(int picture, int x, int y, const int rect[4], int margin, LomRemasterPart parts[], int max)
{
    int count = 0;

    for (int i = 0; i < WSEL_ART_COUNT && count < max; ++i)
    {
        const WselArt* art = &s_wsel_art[i];

        if (art->picture == picture && art_part(WSEL_FILE, art->label, art->ps1, art->hd, art->wide, x, y, rect, margin, &parts[count]))
        {
            ++count;
        }
    }

    return count;
}

/**
 * @brief Make the placements that draw the Remaster's art for one sprite of
 *        one of WSEL's pictures: every part of the art the sprite covers, in
 *        order, chained into one command.
 * @param picture Which picture.
 * @param x Where the picture's top left is on the screen.
 * @param y The same, down.
 * @param rect The sprite: x, y, width and height on the screen.
 * @return The first placement, or -1 when there's nothing to draw.
 */
static int wsel_place(int picture, int x, int y, const int rect[4])
{
    LomRemasterPart parts[WSEL_ART_COUNT];
    return place_parts(parts, lom_remaster_wsel_parts(picture, x, y, rect, lom_native_widescreen_margin(), parts, WSEL_ART_COUNT));
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
#define DAY_NAME_ROW 32
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
 * @note The day names are one 96 by 192 picture, a 32-pixel row a day, and
 *       the Remaster's are the same at 4.5 times, at the left of a wider
 *       picture, in each language, but each name sits 4 pixels higher in its
 *       row than the PS1's, so it's drawn that much lower to stay on the glow
 *       behind it.
 */
int lom_remaster_day_name_part(int language, int u, int v, const int rect[4], LomRemasterPart* part)
{
    if (language <= LOM_REMASTER_TEXT_PS1 || language >= LOM_REMASTER_TEXT_COUNT || v < 0 || v / DAY_NAME_ROW >= DAYS)
    {
        return -1;
    }

    memset(part, 0, sizeof(*part));
    snprintf(part->file, sizeof(part->file), "system/image/%s.psb.m", s_day_name_files[language]);
    part->area[0] = scale(u, DAY_NAME_SCALE_DEN, DAY_NAME_SCALE_NUM);
    part->area[1] = scale(v, DAY_NAME_SCALE_DEN, DAY_NAME_SCALE_NUM);
    part->area[2] = scale(rect[2], DAY_NAME_SCALE_DEN, DAY_NAME_SCALE_NUM);
    part->area[3] = scale(rect[3], DAY_NAME_SCALE_DEN, DAY_NAME_SCALE_NUM);
    set_rect_xy(part, rect[0], rect[1] + DAY_NAME_DROP, rect[0] + rect[2], rect[1] + DAY_NAME_DROP + rect[3]);
    return part->area[0] + part->area[2] <= HIDUKE_W && part->area[1] + part->area[3] <= HIDUKE_H ? 0 : -1;
}

/**
 * @note The PS1 keeps the six days' spirits in their shells as 48 by 56
 *       cells, two to a row, with the empty shell on the fourth row; each of
 *       the Remaster's spirits and its shell is one such cell at about 5.4
 *       times.
 */
int lom_remaster_kameo_part(int shell, int u, int v, const int rect[4], LomRemasterPart* part)
{
    const int column = u / CELL_W, row = v / CELL_H, day = row * 2 + column;
    const char* label = shell ? (row == SHELL_ROW && column == 0 ? "none_k" : NULL) : row < SHELL_ROW && day < DAYS ? s_day_spirits[day] : NULL;

    if (!label)
    {
        return -1;
    }

    memset(part, 0, sizeof(*part));
    snprintf(part->file, sizeof(part->file), "%s", KAMEO_FILE);
    snprintf(part->label, sizeof(part->label), "%s", label);
    part->area[0] = scale(u - column * CELL_W, CELL_W, KAMEO_W);
    part->area[1] = scale(v - row * CELL_H, CELL_H, KAMEO_H);
    part->area[2] = scale(rect[2], CELL_W, KAMEO_W);
    part->area[3] = scale(rect[3], CELL_H, KAMEO_H);
    set_rect_xy(part, rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]);
    return part->area[0] + part->area[2] <= KAMEO_W && part->area[1] + part->area[3] <= KAMEO_H ? 0 : -1;
}

/**
 * @brief Draw the Remaster's art for a part of the world map's status corner.
 * @param code The sprite's code word.
 * @param part LOM_REMASTER_WMAP_*.
 * @note The PS1 draws the spirit, then its shell around it, through the hole
 *       in the shell. The Remaster's shell is solid inside, with the spirit on
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
    LomRemasterPart art;
    read_sprite(words, rect, &u, &v);

    if (part == LOM_REMASTER_WMAP_DAY_NAME)
    {
        if (lom_remaster_day_name_part(names_language(), u, v, rect, &art) == 0)
        {
            const int placement = lom_remaster_picture_place(art.file, NULL, art.area, art.xy, -1, -1);

            if (placement >= 0)
            {
                lom_remaster_picture_command(words, SPRITE_WORDS, placement);
            }
        }

        return;
    }

    const int shell = part == LOM_REMASTER_WMAP_SHELL;
    int w = 0, h = 0;

    if (shell)
    {
        s_status_last = -1;
    }

    if (lom_remaster_kameo_part(shell, u, v, rect, &art) != 0 || !lom_remaster_picture_ready(art.file, art.label, &w, &h) || w != KAMEO_W || h != KAMEO_H)
    {
        return;
    }

    if (shell)
    {
        const int placement = lom_remaster_picture_place(art.file, art.label, art.area, art.xy, -1, -1);

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
    const int placement = lom_remaster_picture_place(art.file, art.label, art.area, art.xy, -1, s_status_last);

    if (placement >= 0)
    {
        lom_remaster_picture_style(placement, flags & 1 ? 0x808080 : (int)(words[0] & 0xFFFFFF), (flags & 2) != 0);
        lom_remaster_picture_command(words, SPRITE_WORDS, -1);
        s_status_last = placement;
    }
}

/**
 * @note A land's name glows green once it's cleared and yellow before, as the
 *       Remaster's two banners do; an artifact's name has its own.
 */
int lom_remaster_land_name_file(int language, int selection, int clut_row, char* file, size_t size)
{
    if (selection < 0 || selection >= 2 * LANDS || language <= LOM_REMASTER_TEXT_PS1 || language >= LOM_REMASTER_TEXT_COUNT)
    {
        return -1;
    }

    const char* kind = selection >= LANDS ? "artifactname" : (clut_row & ~1) == LAND_CLEARED_CLUT_ROW ? "landname" : "landname_c";
    snprintf(file, size, "system/image/land_name/%s/map_%s_%04d.psb.m", s_land_name_folders[language], kind, selection % LANDS + 1);
    return 0;
}

void lom_remaster_land_name_xy(const int text[4], int w, int h, int xy[8])
{
    const int center_x2 = 2 * text[0] + text[2], center_y2 = 2 * text[1] + text[3];
    const int width = w * LABEL_SCALE_NUM / LABEL_SCALE_DEN, height = h * LABEL_SCALE_NUM / LABEL_SCALE_DEN;
    const int x0 = (center_x2 - width) / 2, y0 = (center_y2 - height) / 2, x1 = (center_x2 + width + 1) / 2, y1 = (center_y2 + height + 1) / 2;
    const int points[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    memcpy(xy, points, sizeof(points));
}

/**
 * @brief Draw the Remaster's banner for a land's name on the world map,
 *        centered on where the PS1's name has anything drawn.
 * @param code The sprite's code word.
 * @param selection The land, or 64 plus it for its artifact's name.
 * @param tpage The name's texture page.
 * @note The PS1 adds its name to what's under it, which would wash out the
 *       banner's colors, so the banner is drawn over it instead.
 */
void lom_remaster_wmap_label(void* code, int selection, int tpage)
{
    if (!code || !enabled())
    {
        return;
    }

    uint32_t* words = code;
    int rect[4], u, v;
    char file[96];
    int w = 0, h = 0;
    read_sprite(words, rect, &u, &v);

    if (lom_remaster_land_name_file(names_language(), selection, (int)((words[2] >> 22) & 0x1FF), file, sizeof(file)) != 0 ||
        !lom_remaster_picture_ready(file, NULL, &w, &h))
    {
        return;
    }

    int bounds[4] = {0, 0, rect[2], rect[3]};
    lom_remaster_vram_bounds((tpage & 0xF) * 64, (tpage >> 4 & 1) * 256, u, v, rect[2], rect[3], bounds);
    const int text[4] = {rect[0] + bounds[0], rect[1] + bounds[1], bounds[2], bounds[3]};
    int xy[8];
    lom_remaster_land_name_xy(text, w, h, xy);
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
 * @note The PS1 animates each spirit in its own row of 24 by 24 frames; the
 *       Remaster has its own frames, not always as many, so the PS1's are
 *       spread across them. A spirit's strength in a place is one of 16
 *       16-by-16 orbs, but the game only uses seven: empty for neutral, a
 *       third, two thirds or all of it in the spirit's color for stronger, and
 *       darker empty ones for weaker. The Remaster has no art for those, so
 *       its empty orb is darkened the same way (s_orb_fill and s_orb_shade,
 *       -1 for the orbs the game doesn't use). s_orb_art is where the
 *       Remaster's orb is in its picture; s_orb_area and s_spirit_area are
 *       where the PS1's orb and spirit are in their sprites. The Remaster
 *       names Salamander's two fuller orbs without the m.
 */
int lom_remaster_spirit_part(int spirit, int kind, int u, int v, const int rect[4], LomRemasterPart* part, int* shade)
{
    if (spirit < 0 || spirit >= SPIRITS)
    {
        return -1;
    }

    memset(part, 0, sizeof(*part));
    *shade = 0x80;

    if (kind == LOM_REMASTER_WMAP_SPIRIT_ICON)
    {
        const int frame = u / SPIRIT_FRAME;

        if (rect[2] != SPIRIT_FRAME || rect[3] != SPIRIT_FRAME || v / SPIRIT_FRAME != spirit || frame >= s_spirit_ps1_frames[spirit])
        {
            return -1;
        }

        snprintf(part->file, sizeof(part->file), "%s", SPIRIT_FILE);
        snprintf(part->label, sizeof(part->label), "%s%d", s_spirit_names[spirit], frame * s_spirit_hd_frames[spirit] / s_spirit_ps1_frames[spirit] + 1);
    }
    else
    {
        const int sprite = (u - ORB_U) / ORB * ORB_ROWS + v / ORB;

        if (rect[2] != ORB || rect[3] != ORB || u < ORB_U || sprite < 0 || sprite >= ORB_SPRITES || s_orb_fill[sprite] < 0)
        {
            return -1;
        }

        const int fill = s_orb_fill[sprite];
        snprintf(part->file, sizeof(part->file), "%s", METER_FILE);
        memcpy(part->area, s_orb_art, sizeof(s_orb_art));
        *shade = s_orb_shade[sprite];

        if (fill == 0)
        {
            snprintf(part->label, sizeof(part->label), "zero");
        }
        else
        {
            snprintf(part->label, sizeof(part->label), "%s%d%s", s_spirit_names[spirit], fill, spirit == 4 && fill > 1 ? "" : "m");
        }
    }

    const int* area = kind == LOM_REMASTER_WMAP_SPIRIT_ICON ? s_spirit_area : s_orb_area;
    set_rect_xy(part, rect[0] + area[0], rect[1] + area[1], rect[0] + area[0] + area[2], rect[1] + area[1] + area[3]);
    return 0;
}

/**
 * @brief Draw the Remaster's art for a world map spirit or its strength.
 * @param code The sprite's code word.
 * @param spirit Which spirit, in the game's order.
 * @param kind LOM_REMASTER_WMAP_SPIRIT_*.
 * @note The PS1 marks these sprites semi-transparent as they fade, but their
 *       texels aren't, so they stay solid and only darken, never past the art
 *       as it's painted.
 */
void lom_remaster_wmap_spirit(void* code, int spirit, int kind)
{
    if (!code || !enabled())
    {
        return;
    }

    uint32_t* words = code;
    int rect[4], u, v, shade;
    LomRemasterPart art;
    read_sprite(words, rect, &u, &v);

    if (lom_remaster_spirit_part(spirit, kind, u, v, rect, &art, &shade) != 0)
    {
        return;
    }

    const int placement = lom_remaster_picture_place(art.file, art.label, art.area[2] ? art.area : NULL, art.xy, -1, -1);

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

#define TITLE_FILE "system/image/title/title.psb.m"
#define TITLE_BACKGROUND "background"
#define TITLE_LAYERS 2

static const int s_title_ps1[4] = {0, 0, 320, 224};
static const int s_title_hd[4] = {240, 36, 1440, 1008};
static const char* const s_title_logos[LOM_REMASTER_TEXT_COUNT] = {"logo-en", "logo-en", "logo-en", "logo-en", "logo-en",
                                                                   "logo-ja", "logo-ko", "logo-cn", "logo-tw"};

/**
 * @brief The Remaster's logo for a language: its English one for the
 *        European languages, which it has no logo of their own for.
 * @param language A LOM_REMASTER_TEXT_* other than LOM_REMASTER_TEXT_PS1.
 * @return The logo's label.
 */
static const char* title_logo(int language)
{
    return language > LOM_REMASTER_TEXT_PS1 && language < LOM_REMASTER_TEXT_COUNT ? s_title_logos[language] : s_title_logos[LOM_REMASTER_TEXT_ENGLISH];
}

/**
 * @note The PS1's title backdrop is one picture with its logo painted in. The
 *       Remaster's is a background, painted wide, with a logo in each
 *       language drawn over it, each a whole frame, and both line up with the
 *       PS1's picture at 4.5 times, as WSEL's wall does. Its logo sits higher
 *       than the PS1's, above the menu and the copyright line, which the PS1
 *       still draws: the Remaster only has pictures of its menu for its Asian
 *       languages.
 */
int lom_remaster_title_parts(int language, const int rect[4], int margin, LomRemasterPart parts[], int max)
{
    const char* const labels[TITLE_LAYERS] = {TITLE_BACKGROUND, title_logo(language)};
    int count = 0;

    for (int i = 0; i < TITLE_LAYERS && count < max; ++i)
    {
        if (art_part(TITLE_FILE, labels[i], s_title_ps1, s_title_hd, 1, 0, 0, rect, margin, &parts[count]))
        {
            ++count;
        }
    }

    return count;
}

/**
 * @brief Load the Remaster's title art for the language in use.
 * @return Nonzero when HD pictures are on and the background and the logo
 *         can both be drawn.
 */
static int title_art_ready(void)
{
    return enabled() && lom_remaster_picture_ready(TITLE_FILE, TITLE_BACKGROUND, NULL, NULL) &&
           lom_remaster_picture_ready(TITLE_FILE, title_logo(names_language()), NULL, NULL);
}

void lom_remaster_title_backdrop(void* code, int stride, int count)
{
    if (!code || count <= 0 || !title_art_ready())
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        uint32_t* words = (uint32_t*)((unsigned char*)code + (size_t)i * (size_t)stride);
        const int x0 = (int16_t)(words[1] & 0xFFFF), y0 = (int16_t)(words[1] >> 16);
        const int x3 = (int16_t)(words[7] & 0xFFFF), y3 = (int16_t)(words[7] >> 16);
        const int rect[4] = {x0, y0, x3 - x0, y3 - y0};
        LomRemasterPart parts[TITLE_LAYERS];
        const int placement = place_parts(parts, lom_remaster_title_parts(names_language(), rect, lom_native_widescreen_margin(), parts, TITLE_LAYERS));

        if (placement >= 0)
        {
            lom_remaster_picture_command(words, QUAD_WORDS, placement);
        }
    }
}

int lom_remaster_title_wide(void)
{
    return title_art_ready();
}

#define GNAME_FILE "name_input.psb.m"
#define GNAME_TAB_SHIFT (-60)
#define GNAME_BUTTONS 4
#define GNAME_BUTTON_X 48
#define GNAME_BUTTON_Y 100
#define GNAME_BUTTON_STEP 16
#define GNAME_BUTTON_W 34
#define GNAME_BUTTON_H 8
#define GNAME_CURSOR_LEFT 7
#define GNAME_CURSOR_DROP 3
#define GNAME_FIRST_BUTTON_POSITION 2
#define GNAME_FIRST_TAB_POSITION 6
#define GNAME_TABS 4
#define GNAME_GLYPH_OK 30
#define GNAME_GLYPH_DELETE 31
#define GNAME_GLYPH_UNDO 33
#define GNAME_GLYPH_FIRST_TAB 26
#define GNAME_GLYPH_LAST_TAB 29
#define GNAME_GLYPH_FIRST_ORNAMENT 36
#define GNAME_GLYPH_LAST_ORNAMENT 38
#define GNAME_ARROW_U 9
#define GNAME_ARROW_V 4
#define GNAME_ARROW_W 21
#define GNAME_ARROW_H 9

typedef struct
{
    int glyph;
    const char* label;
    int x, y, w, h;
} GnameArt;

static const GnameArt s_gname_art[] = {
    {2, "typewriter_paper", 1, 0, 190, 33},       {3, "typewriter", 0, 0, 64, 56},
    {14, "typewriter_anim_paper1", 1, 3, 20, 34}, {15, "typewriter_anim_paper2", 2, 3, 20, 34},
    {16, "typewriter_anim_paper3", 1, 4, 20, 34}, {17, "typewriter_anim_paper4", -1, 4, 20, 34},
    {24, "typewriter_anim_hand1", 5, 1, 20, 23},  {25, "typewriter_anim_hand2", 6, 4, 20, 23},
};

#define GNAME_ART_COUNT ((int)(sizeof(s_gname_art) / sizeof(s_gname_art[0])))

static const char* const s_gname_backgrounds[LOM_REMASTER_TEXT_COUNT] = {"bg_en", "bg_en", "bg_de", "bg_fr", "bg_es", "bg_jp", "bg_kr", "bg_ch", "bg_tw"};

static int s_gname_layout;

/**
 * @brief The Remaster's name entry background for a language, which has its
 *        buttons painted in.
 * @param language A LOM_REMASTER_TEXT_* other than LOM_REMASTER_TEXT_PS1.
 * @return The background's label.
 */
static const char* gname_background(int language)
{
    return language > LOM_REMASTER_TEXT_PS1 && language < LOM_REMASTER_TEXT_COUNT ? s_gname_backgrounds[language]
                                                                                  : s_gname_backgrounds[LOM_REMASTER_TEXT_ENGLISH];
}

/**
 * @note The Remaster's name entry is painted for a wide frame: its panel is
 *       wider than a 4:3 picture, with OK, delete (a keyboard on the
 *       Remaster), AUTO and UNDO painted in a column at its left, where the
 *       PS1 has them in a row above it. Its typewriter, the paper the name is
 *       typed on and the typing animation's paper and hands are the PS1's
 *       pictures repainted, at 4.5 times, each placed as the PS1's sits in its
 *       glyph's box, which the repaint is cut tighter than.
 */
int lom_remaster_gname_part(int glyph, const int rect[4], LomRemasterPart* part)
{
    for (int i = 0; i < GNAME_ART_COUNT; ++i)
    {
        const GnameArt* art = &s_gname_art[i];

        if (art->glyph == glyph)
        {
            memset(part, 0, sizeof(*part));
            snprintf(part->file, sizeof(part->file), "%s", GNAME_FILE);
            snprintf(part->label, sizeof(part->label), "%s", art->label);
            set_rect_xy(part, rect[0] + art->x, rect[1] + art->y, rect[0] + art->x + art->w, rect[1] + art->y + art->h);
            return 1;
        }
    }

    return 0;
}

int lom_remaster_gname_cursor_at(int position, int* x, int* y)
{
    if (position >= GNAME_FIRST_BUTTON_POSITION && position < GNAME_FIRST_BUTTON_POSITION + GNAME_BUTTONS)
    {
        *x = GNAME_BUTTON_X - GNAME_CURSOR_LEFT;
        *y = GNAME_BUTTON_Y + (position - GNAME_FIRST_BUTTON_POSITION) * GNAME_BUTTON_STEP + GNAME_CURSOR_DROP;
        return 1;
    }

    if (position >= GNAME_FIRST_TAB_POSITION && position < GNAME_FIRST_TAB_POSITION + GNAME_TABS)
    {
        *x += GNAME_TAB_SHIFT;
        return 1;
    }

    return 0;
}

/**
 * @brief Load the Remaster's name entry art.
 * @return Nonzero when its background, in the language in use, and every
 *         picture that replaces one of the PS1's can be drawn.
 */
static int gname_art_ready(void)
{
    if (!lom_remaster_picture_ready(GNAME_FILE, gname_background(names_language()), NULL, NULL))
    {
        return 0;
    }

    for (int i = 0; i < GNAME_ART_COUNT; ++i)
    {
        if (!lom_remaster_picture_ready(GNAME_FILE, s_gname_art[i].label, NULL, NULL))
        {
            return 0;
        }
    }

    return 1;
}

int lom_remaster_gname_begin(void)
{
    s_gname_layout = enabled() && lom_native_widescreen_enabled() && gname_art_ready();
    return s_gname_layout;
}

void lom_remaster_gname_end(void)
{
    s_gname_layout = 0;
}

int lom_remaster_gname_layout(void)
{
    return s_gname_layout;
}

void lom_remaster_gname_background(void* code, int stride, int count)
{
    if (!code || count <= 0 || !s_gname_layout)
    {
        return;
    }

    const int whole[4] = {s_title_ps1[0], s_title_ps1[1], s_title_ps1[2], s_title_ps1[3]};
    LomRemasterPart part;
    const int placement =
        art_part(GNAME_FILE, gname_background(names_language()), s_title_ps1, s_title_hd, 1, 0, 0, whole, lom_native_widescreen_margin(), &part)
            ? place_parts(&part, 1)
            : -1;

    for (int i = 0; i < count; ++i)
    {
        lom_remaster_picture_command((unsigned char*)code + (size_t)i * (size_t)stride, SPRITE_WORDS, i == 0 ? placement : -1);
    }
}

/**
 * @brief Move a sprite across.
 * @param words The sprite's words from its code word.
 * @param dx How far.
 */
static void shift_sprite(uint32_t* words, int dx)
{
    const int x = (int16_t)(words[1] & 0xFFFF) + dx;
    words[1] = (words[1] & 0xFFFF0000u) | ((uint32_t)x & 0xFFFF);
}

/**
 * @brief One of a glyph's sprites.
 * @param code The first sprite's code word.
 * @param stride How far each next sprite's is, in bytes.
 * @param index Which.
 * @return Its words from its code word.
 */
static uint32_t* glyph_sprite(void* code, int stride, int index)
{
    return (uint32_t*)((unsigned char*)code + (size_t)index * (size_t)stride);
}

void lom_remaster_gname_glyph(void* code, int sprites, int stride, int glyph)
{
    if (!code || sprites <= 0 || !s_gname_layout)
    {
        return;
    }

    uint32_t* words = code;

    if (glyph >= GNAME_GLYPH_OK && glyph <= GNAME_GLYPH_UNDO)
    {
        for (int i = glyph == GNAME_GLYPH_DELETE ? 1 : 0; i < sprites; ++i)
        {
            lom_remaster_picture_command(glyph_sprite(code, stride, i), SPRITE_WORDS, -1);
        }

        if (glyph == GNAME_GLYPH_DELETE)
        {
            const int x = GNAME_BUTTON_X + (GNAME_BUTTON_W - GNAME_ARROW_W) / 2;
            const int y = GNAME_BUTTON_Y + GNAME_BUTTON_STEP;
            const unsigned u = (words[2] & 0xFF) + GNAME_ARROW_U, v = ((words[2] >> 8) & 0xFF) + GNAME_ARROW_V;
            words[1] = ((uint32_t)y << 16) | ((uint32_t)x & 0xFFFF);
            words[2] = (words[2] & 0xFFFF0000u) | (v & 0xFF) << 8 | (u & 0xFF);
            words[3] = (uint32_t)GNAME_ARROW_H << 16 | GNAME_ARROW_W;
        }

        return;
    }

    if ((glyph >= GNAME_GLYPH_FIRST_TAB && glyph <= GNAME_GLYPH_LAST_TAB) || (glyph >= GNAME_GLYPH_FIRST_ORNAMENT && glyph <= GNAME_GLYPH_LAST_ORNAMENT))
    {
        for (int i = 0; i < sprites; ++i)
        {
            shift_sprite(glyph_sprite(code, stride, i), GNAME_TAB_SHIFT);
        }

        return;
    }

    const int rect[4] = {(int16_t)(words[1] & 0xFFFF), (int16_t)(words[1] >> 16), (int)(words[3] & 0xFFFF), (int)(words[3] >> 16)};
    LomRemasterPart part;

    if (lom_remaster_gname_part(glyph, rect, &part))
    {
        lom_remaster_picture_command(words, SPRITE_WORDS, lom_remaster_picture_place(part.file, part.label, NULL, part.xy, -1, -1));
    }
}

int lom_remaster_gname_tab_shift(void)
{
    return s_gname_layout ? GNAME_TAB_SHIFT : 0;
}

void lom_remaster_gname_cursor(int position, int* x, int* y)
{
    if (s_gname_layout && x && y)
    {
        lom_remaster_gname_cursor_at(position, x, y);
    }
}
