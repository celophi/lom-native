#include "../../../src/remaster/remaster_internal.h"
#include "lom_native_remaster.h"
#include <stdio.h>
#include <string.h>
#include "../../support/lom_check.h"

#define WSEL_LAND_MAP 0
#define WSEL_WORLD_MAP 1
#define WSEL_CURSOR 2
#define WSEL_HERO 4
#define WIDE_MARGIN 53
#define DAYS 6
#define DAY_ROW 32
#define HD_DAY_ROW 144

static const int s_hd_name_tops[DAYS] = {10, 153, 297, 443, 579, 729};

/**
 * @brief Check a part covers a rectangle on the screen.
 * @param part The part.
 * @param x0 The rectangle's left.
 * @param y0 Its top.
 * @param x1 Its right.
 * @param y1 Its bottom.
 */
static void covers(const LomRemasterPart* part, int x0, int y0, int x1, int y1)
{
    const int xy[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    CHECK(memcmp(part->xy, xy, sizeof(xy)) == 0);
}

/**
 * @brief Check a part takes a rectangle of its picture.
 * @param part The part.
 * @param x The rectangle's left.
 * @param y Its top.
 * @param w Its width.
 * @param h Its height.
 */
static void takes(const LomRemasterPart* part, int x, int y, int w, int h)
{
    CHECK(part->area[0] == x && part->area[1] == y && part->area[2] == w && part->area[3] == h);
}

/**
 * @brief Check WSEL's wall, the map on it, the parchment and the cursor come
 *        from where they were lined up, and that in a wide frame the wall's
 *        outer strips reach into the sides without leaving its picture.
 */
static void wsel_pictures_come_from_where_they_were_lined_up(void)
{
    LomRemasterPart parts[4];
    const int first[4] = {0, 0, 128, 224};
    const int middle[4] = {128, 0, 128, 224};
    const int last[4] = {256, 0, 64, 224};

    CHECK(lom_remaster_wsel_parts(WSEL_WORLD_MAP, 0, 0, first, 0, parts, 4) == 2);
    CHECK(!strcmp(parts[0].file, "map_select.psb.m") && !strcmp(parts[0].label, "map_select_bg"));
    takes(&parts[0], 240, 36, 576, 1008);
    covers(&parts[0], 0, 0, 128, 224);
    CHECK(!strcmp(parts[1].label, "map_select_map"));
    takes(&parts[1], 5, 5, 141, 527);
    covers(&parts[1], 96, 48, 128, 168);

    CHECK(lom_remaster_wsel_parts(WSEL_WORLD_MAP, 0, 0, middle, 0, parts, 4) == 2);
    CHECK(!strcmp(parts[0].label, "map_select_bg") && !strcmp(parts[1].label, "map_select_map"));
    takes(&parts[1], 146, 5, 422, 527);
    covers(&parts[1], 128, 48, 224, 168);

    CHECK(lom_remaster_wsel_parts(WSEL_WORLD_MAP, 0, 0, first, WIDE_MARGIN, parts, 4) == 2);
    CHECK(parts[0].area[0] == 2);
    covers(&parts[0], -WIDE_MARGIN, 0, 128, 224);
    CHECK(lom_remaster_wsel_parts(WSEL_WORLD_MAP, 0, 0, last, WIDE_MARGIN, parts, 4) == 1);
    CHECK(parts[0].area[0] + parts[0].area[2] == 1919);
    covers(&parts[0], 256, 0, 320 + WIDE_MARGIN, 224);
    CHECK(lom_remaster_wsel_parts(WSEL_WORLD_MAP, 0, 0, middle, WIDE_MARGIN, parts, 4) == 2);
    covers(&parts[0], 128, 0, 256, 224);

    const int land[4] = {-64, -112, 128, 256};
    CHECK(lom_remaster_wsel_parts(WSEL_LAND_MAP, -64, -112, land, WIDE_MARGIN, parts, 4) == 1);
    CHECK(!strcmp(parts[0].label, "worldmap_map"));
    takes(&parts[0], 0, 0, 591, 1182);

    const int cursor[4] = {101, 53, 120, 118};
    CHECK(lom_remaster_wsel_parts(WSEL_CURSOR, 101, 53, cursor, 0, parts, 4) == 1);
    CHECK(!strcmp(parts[0].label, "worldmap_frame"));
    takes(&parts[0], 0, 0, 500, 500);

    const int hero[4] = {216, 180, 32, 40};
    CHECK(lom_remaster_wsel_parts(WSEL_HERO, 216, 180, hero, 0, parts, 4) == 0);
}

/**
 * @brief Check each day's name comes from its own row of the Remaster's
 *        picture, which holds that day's name and none of the next day's,
 *        lowered onto the PS1's glow, in the language's picture.
 *        s_hd_name_tops is where each name starts in the Remaster's
 *        pictures, measured from its files: about 144 pixels apart, the same
 *        in every language.
 */
static void day_names_come_from_their_own_rows(void)
{
    LomRemasterPart part;

    for (int day = 0; day < DAYS; ++day)
    {
        const int rect[4] = {168, 16, 96, DAY_ROW};
        CHECK(lom_remaster_day_name_part(LOM_REMASTER_TEXT_ENGLISH, 0, day * DAY_ROW, rect, &part) == 0);
        takes(&part, 0, day * HD_DAY_ROW, 432, HD_DAY_ROW);
        CHECK(part.area[1] <= s_hd_name_tops[day] && s_hd_name_tops[day] < part.area[1] + part.area[3]);
        CHECK(day + 1 == DAYS || part.area[1] + part.area[3] <= s_hd_name_tops[day + 1]);
        covers(&part, 168, 20, 264, 52);
    }

    const int rect[4] = {168, 16, 96, DAY_ROW};
    CHECK(lom_remaster_day_name_part(LOM_REMASTER_TEXT_ENGLISH, 0, 0, rect, &part) == 0 && !strcmp(part.file, "system/image/hiduke_en.psb.m"));
    CHECK(lom_remaster_day_name_part(LOM_REMASTER_TEXT_GERMAN, 0, 0, rect, &part) == 0 && !strcmp(part.file, "system/image/hiduke_ge.psb.m"));
    CHECK(lom_remaster_day_name_part(LOM_REMASTER_TEXT_JAPANESE, 0, 0, rect, &part) == 0 && !strcmp(part.file, "system/image/hiduke.psb.m"));
    CHECK(lom_remaster_day_name_part(LOM_REMASTER_TEXT_CHINESE_TRADITIONAL, 0, 0, rect, &part) == 0 && !strcmp(part.file, "system/image/hiduke_cn.psb.m"));
    CHECK(lom_remaster_day_name_part(LOM_REMASTER_TEXT_PS1, 0, 0, rect, &part) != 0);
    CHECK(lom_remaster_day_name_part(LOM_REMASTER_TEXT_ENGLISH, 0, 6 * DAY_ROW, rect, &part) != 0);
}

/**
 * @brief Check the shell and each day's spirit come from their own pictures,
 *        the spirit's part being the one the PS1's sprite shows of its cell.
 */
static void shell_and_spirits_come_from_their_cells(void)
{
    LomRemasterPart part;
    const int shell[4] = {262, 14, 48, 56};
    const int icon[4] = {270, 28, 33, 28};

    CHECK(lom_remaster_kameo_part(1, 0, 168, shell, &part) == 0);
    CHECK(!strcmp(part.file, "kameo.psb.m") && !strcmp(part.label, "none_k"));
    takes(&part, 0, 0, 260, 300);
    covers(&part, 262, 14, 310, 70);

    CHECK(lom_remaster_kameo_part(0, 56, 70, icon, &part) == 0 && !strcmp(part.label, "jinn_k"));
    takes(&part, 43, 75, 179, 150);
    covers(&part, 270, 28, 303, 56);
    CHECK(lom_remaster_kameo_part(0, 8, 14, icon, &part) == 0 && !strcmp(part.label, "salamander_k"));
    CHECK(lom_remaster_kameo_part(0, 56, 126, icon, &part) == 0 && !strcmp(part.label, "undine_k"));

    CHECK(lom_remaster_kameo_part(1, 8, 14, icon, &part) != 0);
    CHECK(lom_remaster_kameo_part(0, 0, 168, shell, &part) != 0);
}

/**
 * @brief Check a land's name comes from the banner for how far along it is,
 *        in the language's folder, and that the banner is centered on the
 *        PS1's name at 4.5 times.
 */
static void land_names_come_from_the_right_banner(void)
{
    char file[96];

    CHECK(lom_remaster_land_name_file(LOM_REMASTER_TEXT_ENGLISH, 0, 506, file, sizeof(file)) == 0);
    CHECK(!strcmp(file, "system/image/land_name/US/map_landname_0001.psb.m"));
    CHECK(lom_remaster_land_name_file(LOM_REMASTER_TEXT_ENGLISH, 0, 507, file, sizeof(file)) == 0);
    CHECK(!strcmp(file, "system/image/land_name/US/map_landname_0001.psb.m"));
    CHECK(lom_remaster_land_name_file(LOM_REMASTER_TEXT_ENGLISH, 24, 509, file, sizeof(file)) == 0);
    CHECK(!strcmp(file, "system/image/land_name/US/map_landname_c_0025.psb.m"));
    CHECK(lom_remaster_land_name_file(LOM_REMASTER_TEXT_FRENCH, 64, 506, file, sizeof(file)) == 0);
    CHECK(!strcmp(file, "system/image/land_name/FR/map_artifactname_0001.psb.m"));
    CHECK(lom_remaster_land_name_file(LOM_REMASTER_TEXT_CHINESE_TRADITIONAL, 1, 506, file, sizeof(file)) == 0);
    CHECK(!strcmp(file, "system/image/land_name/TW/map_landname_0002.psb.m"));
    CHECK(lom_remaster_land_name_file(LOM_REMASTER_TEXT_ENGLISH, 128, 506, file, sizeof(file)) != 0);
    CHECK(lom_remaster_land_name_file(LOM_REMASTER_TEXT_PS1, 0, 506, file, sizeof(file)) != 0);

    const int text[4] = {200, 100, 40, 12};
    const int expected[8] = {192, 98, 248, 98, 192, 114, 248, 114};
    int xy[8];
    lom_remaster_land_name_xy(text, 252, 72, xy);
    CHECK(memcmp(xy, expected, sizeof(xy)) == 0);
}

/**
 * @brief Check every frame of every spirit comes from one of the Remaster's
 *        frames for it, in order from its first (spirits with fewer of the
 *        Remaster's frames than the PS1's share some), and each strength the
 *        game uses from the right orb,
 *        the weaker ones darkened, and the unused ones not at all.
 */
static void spirits_and_strengths_come_from_their_frames(void)
{
    static const char* const names[8] = {"wilowisp", "sheid", "dryad", "aura", "salamander", "gnome", "jinn", "undine"};
    static const int ps1_frames[8] = {4, 4, 4, 4, 4, 5, 4, 3};
    static const int hd_frames[8] = {4, 3, 4, 4, 4, 6, 5, 4};
    LomRemasterPart part;
    int shade;
    const int icon[4] = {10, 134, 24, 24};

    for (int spirit = 0; spirit < 8; ++spirit)
    {
        int previous = 0;

        for (int frame = 0; frame < ps1_frames[spirit]; ++frame)
        {
            int number = 0;
            char name[32];
            CHECK(lom_remaster_spirit_part(spirit, LOM_REMASTER_WMAP_SPIRIT_ICON, frame * 24, spirit * 24, icon, &part, &shade) == 0);
            CHECK(!strcmp(part.file, "spirit_anime.psb.m") && part.area[2] == 0 && shade == 0x80);
            CHECK(sscanf(part.label, "%31[a-z]%d", name, &number) == 2 && !strcmp(name, names[spirit]));
            CHECK(number >= 1 && number <= hd_frames[spirit] && number >= previous && (frame > 0 || number == 1));
            previous = number;
            covers(&part, 12, 136, 32, 156);
        }

        CHECK(lom_remaster_spirit_part(spirit, LOM_REMASTER_WMAP_SPIRIT_ICON, ps1_frames[spirit] * 24, spirit * 24, icon, &part, &shade) != 0);
        CHECK(lom_remaster_spirit_part(spirit, LOM_REMASTER_WMAP_SPIRIT_ICON, 0, ((spirit + 1) % 8) * 24, icon, &part, &shade) != 0);
    }

    const int orb[4] = {14, 154, 16, 16};

    static const struct
    {
        int sprite, spirit;
        const char* label;
        int shade;
    } orbs[] = {{0, 0, "zero", 0x80},         {5, 3, "aura1m", 0x80},       {10, 3, "aura2m", 0x80}, {15, 3, "aura3m", 0x80}, {10, 4, "salamander2", 0x80},
                {15, 4, "salamander3", 0x80}, {5, 4, "salamander1m", 0x80}, {4, 7, "zero", 0x60},    {8, 7, "zero", 0x40},    {12, 7, "zero", 0x20}};

    for (size_t i = 0; i < sizeof(orbs) / sizeof(orbs[0]); ++i)
    {
        const int u = 128 + orbs[i].sprite / 4 * 16, v = orbs[i].sprite % 4 * 16;
        CHECK(lom_remaster_spirit_part(orbs[i].spirit, LOM_REMASTER_WMAP_SPIRIT_LEVEL, u, v, orb, &part, &shade) == 0);
        CHECK(!strcmp(part.file, "spirit_meter.psb.m") && !strcmp(part.label, orbs[i].label) && shade == orbs[i].shade);
        takes(&part, 30, 30, 68, 68);
        covers(&part, 16, 156, 29, 168);
    }

    for (int sprite = 0; sprite < 16; ++sprite)
    {
        const int used = sprite == 0 || sprite == 4 || sprite == 5 || sprite == 8 || sprite == 10 || sprite == 12 || sprite == 15;
        CHECK((lom_remaster_spirit_part(0, LOM_REMASTER_WMAP_SPIRIT_LEVEL, 128 + sprite / 4 * 16, sprite % 4 * 16, orb, &part, &shade) == 0) == used);
    }

    CHECK(lom_remaster_spirit_part(8, LOM_REMASTER_WMAP_SPIRIT_ICON, 0, 0, icon, &part, &shade) != 0);
}

/**
 * @brief Check the title's backdrop strips take the Remaster's background and
 *        then its logo from the same place, the PS1's picture at 4.5 times,
 *        that the outer strips reach into a wide frame's sides, that the logo
 *        follows the language, and that a quad off the backdrop takes nothing.
 */
static void title_backdrop_comes_from_the_background_and_logo(void)
{
    LomRemasterPart parts[2];
    const int first[4] = {0, 0, 64, 224};
    const int last[4] = {256, 0, 64, 224};
    const int below[4] = {0, 224, 64, 16};

    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_ENGLISH, first, 0, parts, 2) == 2);
    CHECK(!strcmp(parts[0].file, "system/image/title/title.psb.m") && !strcmp(parts[0].label, "background"));
    CHECK(!strcmp(parts[1].file, parts[0].file) && !strcmp(parts[1].label, "logo-en"));
    takes(&parts[0], 240, 36, 288, 1008);
    takes(&parts[1], 240, 36, 288, 1008);
    covers(&parts[0], 0, 0, 64, 224);
    covers(&parts[1], 0, 0, 64, 224);

    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_ENGLISH, first, WIDE_MARGIN, parts, 2) == 2);
    takes(&parts[0], 2, 36, 526, 1008);
    covers(&parts[0], -WIDE_MARGIN, 0, 64, 224);
    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_ENGLISH, last, WIDE_MARGIN, parts, 2) == 2);
    takes(&parts[1], 1392, 36, 527, 1008);
    covers(&parts[1], 256, 0, 320 + WIDE_MARGIN, 224);

    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_PS1, first, 0, parts, 2) == 2 && !strcmp(parts[1].label, "logo-en"));
    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_GERMAN, first, 0, parts, 2) == 2 && !strcmp(parts[1].label, "logo-en"));
    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_JAPANESE, first, 0, parts, 2) == 2 && !strcmp(parts[1].label, "logo-ja"));
    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_KOREAN, first, 0, parts, 2) == 2 && !strcmp(parts[1].label, "logo-ko"));
    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_CHINESE_SIMPLIFIED, first, 0, parts, 2) == 2 && !strcmp(parts[1].label, "logo-cn"));
    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_CHINESE_TRADITIONAL, first, 0, parts, 2) == 2 && !strcmp(parts[1].label, "logo-tw"));
    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_ENGLISH, first, 0, parts, 1) == 1 && !strcmp(parts[0].label, "background"));
    CHECK(lom_remaster_title_parts(LOM_REMASTER_TEXT_ENGLISH, below, 0, parts, 2) == 0);
}

/**
 * @brief Check name entry's typewriter, paper and typing animation take the
 *        Remaster's pictures, placed where the PS1's sits in each glyph's box,
 *        that other glyphs keep the PS1's, and that the cursor's targets for
 *        the buttons go left of the painted column and for the tabs onto the
 *        wood, leaving the letters' alone.
 */
static void name_entry_takes_the_remasters_pictures_and_layout(void)
{
    LomRemasterPart part;
    const int typewriter[4] = {232, 4, 64, 56};
    const int paper[4] = {248, 4, 24, 40};
    const int hand[4] = {280, 12, 24, 24};

    CHECK(lom_remaster_gname_part(3, typewriter, &part) == 1);
    CHECK(!strcmp(part.file, "name_input.psb.m") && !strcmp(part.label, "typewriter"));
    covers(&part, 232, 4, 296, 60);
    CHECK(lom_remaster_gname_part(2, typewriter, &part) == 1 && !strcmp(part.label, "typewriter_paper"));
    covers(&part, 233, 4, 423, 37);
    CHECK(lom_remaster_gname_part(14, paper, &part) == 1 && !strcmp(part.label, "typewriter_anim_paper1"));
    covers(&part, 249, 7, 269, 41);
    CHECK(lom_remaster_gname_part(17, paper, &part) == 1 && !strcmp(part.label, "typewriter_anim_paper4"));
    covers(&part, 247, 8, 267, 42);
    CHECK(lom_remaster_gname_part(25, hand, &part) == 1 && !strcmp(part.label, "typewriter_anim_hand2"));
    covers(&part, 286, 16, 306, 39);
    CHECK(lom_remaster_gname_part(20, hand, &part) == 0);
    CHECK(lom_remaster_gname_part(31, hand, &part) == 0);

    for (int button = 0; button < 4; ++button)
    {
        int x = 64 + 52 * button, y = 64;
        CHECK(lom_remaster_gname_cursor_at(2 + button, &x, &y) == 1 && x == 41 && y == 103 + 16 * button);
    }

    int x = 8, y = 108;
    CHECK(lom_remaster_gname_cursor_at(6, &x, &y) == 1 && x == -52 && y == 108);
    x = 8;
    y = 165;
    CHECK(lom_remaster_gname_cursor_at(9, &x, &y) == 1 && x == -52 && y == 165);
    x = 84;
    y = 104;
    CHECK(lom_remaster_gname_cursor_at(10, &x, &y) == 0 && x == 84 && y == 104);
    CHECK(lom_remaster_gname_cursor_at(1, &x, &y) == 0 && x == 84 && y == 104);
}

int main(void)
{
    wsel_pictures_come_from_where_they_were_lined_up();
    day_names_come_from_their_own_rows();
    shell_and_spirits_come_from_their_cells();
    land_names_come_from_the_right_banner();
    spirits_and_strengths_come_from_their_frames();
    title_backdrop_comes_from_the_background_and_logo();
    name_entry_takes_the_remasters_pictures_and_layout();
    puts("remaster-screens: WSEL, the world map, the title and name entry take the Remaster's art from where it was lined up");
    return 0;
}
