#include "common/card_menu.h"
#include "lom_native_card.h"
#include "lom_native_draw.h"

#define CARD_MENU_ROWS_TOP 38
#define CARD_MENU_ROW_HEIGHT 35
#define CARD_MENU_ROWS_SHOWN 4
#define CARD_MENU_ROW_LEFT (-9)
#define CARD_MENU_ROW_WIDTH 344
#define CARD_MENU_ROW_LABEL_RIGHT_X 24
#define CARD_MENU_ROW_LABEL_BASELINE 22
#define CARD_MENU_ROW_FACE_X 26
#define CARD_MENU_ROW_FACE_Y 2
#define CARD_MENU_ROW_FACE_SIZE 31
#define CARD_MENU_ROW_TEXT_X 81
#define CARD_MENU_ROW_PLACE_X 140
#define CARD_MENU_ROW_RIGHT_X 303
#define CARD_MENU_ROW_LINE1_BASELINE 12
#define CARD_MENU_ROW_LINE2_BASELINE 28
#define CARD_MENU_ROW_LINE_BYTES 48
#define CARD_MENU_ROW_ARROW_X 152
#define CARD_MENU_ROW_ARROW_W 16
#define CARD_MENU_ROW_ARROW_H 9
#define CARD_MENU_ROW_ARROW_UP_Y 28
#define CARD_MENU_ROW_ARROW_DOWN_Y 177

typedef struct
{
    char name[sizeof(g_card_entries[0][0].name)];
    s32 slot;
    s32 known;
    s32 hero;
    s32 play_time;
    s32 land;
    u8 player[SAVED_GAME_SUMMARY_NAME_BYTES];
} CardMenuRowSummary;

static CardMenuRowSummary s_card_menu_rows[CARD_MENU_ENTRY_COUNT_LIMIT];
static s32 s_card_menu_rows_top;
static SaveFile s_card_menu_row_file;

/**
 * @brief For the port's Remaster layout, read what a save's row shows: the hero, play time, place and name. Each save
 *        is read once, from the card's file, without the card's events, so the game's own reads go on as they were.
 * @param index The save's place in the list.
 * @return The row's summary; @c known is zero when it isn't one of this game's saves or can't be read.
 */
static const CardMenuRowSummary* card_menu_row_summary(s32 index)
{
    CardMenuRowSummary* row = &s_card_menu_rows[index];
    const char* name = (const char*)g_card_entries[g_card_slot][index].name;
    char path[CARD_DEVICE_BYTES + sizeof(row->name)];

    if (row->slot == g_card_slot && strncmp(row->name, name, sizeof(row->name)) == 0)
    {
        return row;
    }

    memset(row, 0, sizeof(*row));
    strncpy(row->name, name, sizeof(row->name));
    row->slot = g_card_slot;

    if (strncmp(g_lom_save_filename_prefix, name, CARD_SAVE_FILENAME_PREFIX_LENGTH) != 0)
    {
        return row;
    }

    strcpy(path, CARD_DEVICE_PREFIX);
    path[CARD_DEVICE_SLOT_DIGIT - 1] += g_card_slot;
    strncat(path, name, sizeof(row->name));

    if (lom_native_card_peek(path, &s_card_menu_row_file, CARD_MENU_ENTRY_READ_BYTES) == CARD_MENU_ENTRY_READ_BYTES &&
        (s_card_menu_row_file.saved_game.compatibility_tag == SAVE_TAG_ANY || s_card_menu_row_file.saved_game.compatibility_tag == g_save_compatibility_tag))
    {
        row->known = 1;
        row->hero = s_card_menu_row_file.saved_game.spawn.bits.party_icon_0;
        row->play_time = s_card_menu_row_file.saved_game.play_time;
        row->land = s_card_menu_row_file.saved_game.location.bits.current_land;
        memcpy(row->player, s_card_menu_row_file.saved_game.summary_name, sizeof(row->player));
        row->player[sizeof(row->player) - 1] = 0;
    }

    return row;
}

/**
 * @brief Write a row's line in white, as the Remaster's.
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry.
 * @param text The text.
 * @param x Its left or right.
 * @param baseline Its baseline.
 * @param align FIELD_TEXT_ALIGN_*.
 * @return Advanced primitive-buffer cursor.
 */
static void* card_menu_draw_row_text(void* prim, OT_TYPE* ot, u8* text, s32 x, s32 baseline, s32 align)
{
    return card_menu_draw_hd_text(prim, ot, text, x, baseline, align, LOM_CARDS_TEXT_ROW);
}

/**
 * @brief Add the game's text to a line being put together.
 * @param out The line.
 * @param at Where in it the text goes, moved past it.
 * @param room Bytes the line has.
 * @param text The text.
 */
static void card_menu_append_text(u8* out, s32* at, s32 room, const u8* text)
{
    while (*text != 0 && *at < room - 1)
    {
        out[(*at)++] = *text++;
    }

    out[*at] = 0;
}

/**
 * @brief Add a number with at least two digits to a line being put together, as the Remaster writes its numbers.
 * @param out The line.
 * @param at Where in it the number goes, moved past it.
 * @param room Bytes the line has.
 * @param value The number, 0 or more.
 */
static void card_menu_append_number(u8* out, s32* at, s32 room, s32 value)
{
    u8 digits[12];
    s32 count = 0;

    do
    {
        digits[count++] = '0' + value % 10;
        value /= 10;
    } while (value != 0 && count < (s32)sizeof(digits));

    if (count < 2)
    {
        digits[count++] = '0';
    }

    while (count > 0 && *at < room - 1)
    {
        out[(*at)++] = digits[--count];
    }

    out[*at] = 0;
}

/**
 * @brief Draw one save's row in the Remaster's layout: its number ("No. 01") or what it is, and for one of this game's
 *        saves the hero's face, the play time ("01:08"), the place and the hero's name; Newest or Oldest, and a + for a
 *        cleared game.
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry.
 * @param index The save's place in the list.
 * @param y The row's top.
 * @return Advanced primitive-buffer cursor.
 */
static void* card_menu_draw_remaster_row(void* prim, OT_TYPE* ot, s32 index, s32 y)
{
    u16* text_table = CARD_MENU_REMASTER_TEXT_TABLE;
    const char* name = (const char*)g_card_entries[g_card_slot][index].name;
    const CardMenuRowSummary* row = card_menu_row_summary(index);
    u8 line[CARD_MENU_ROW_LINE_BYTES];
    s32 at;

    if (g_card_entry_ranks[index] >= 0)
    {
        at = 0;
        card_menu_append_text(line, &at, sizeof(line), CARD_MENU_TEXT_BY_OFFSET(text_table, g_card_menu_text_number_label));
        if (at > 0 && line[at - 1] != ' ')
        {
            card_menu_append_text(line, &at, sizeof(line), (const u8*)" ");
        }

        card_menu_append_number(line, &at, sizeof(line), g_card_entry_suffix_values[index]);
        prim = card_menu_draw_row_text(prim, ot, line, CARD_MENU_ROW_LABEL_RIGHT_X, y + CARD_MENU_ROW_LABEL_BASELINE, FIELD_TEXT_ALIGN_RIGHT);

        if ((g_card_rank_count - 1) == g_card_entry_ranks[index])
        {
            prim = card_menu_draw_row_text(prim, ot, CARD_MENU_TEXT_BY_OFFSET(text_table, text_table[CARD_MENU_TEXT_NEWEST]), CARD_MENU_ROW_RIGHT_X,
                                           y + CARD_MENU_ROW_LINE1_BASELINE, FIELD_TEXT_ALIGN_RIGHT);
        }
        else if (g_card_entry_ranks[index] < 2)
        {
            prim = card_menu_draw_row_text(prim, ot, CARD_MENU_TEXT_BY_OFFSET(text_table, text_table[CARD_MENU_TEXT_OLDEST]), CARD_MENU_ROW_RIGHT_X,
                                           y + CARD_MENU_ROW_LINE1_BASELINE, FIELD_TEXT_ALIGN_RIGHT);
        }

        if (*skip_hex_digits(&g_card_entries[g_card_slot][index].name[CARD_SAVE_FILENAME_PREFIX_LENGTH]) == '+')
        {
            prim = card_menu_draw_row_text(prim, ot, CARD_MENU_TEXT_BY_OFFSET(text_table, g_card_menu_text_plus_marker), CARD_MENU_ROW_RIGHT_X,
                                           y + CARD_MENU_ROW_LINE2_BASELINE, FIELD_TEXT_ALIGN_RIGHT);
        }
    }
    else if (strncmp(g_lom_pocketstation_filename_prefix, name, CARD_SAVE_FILENAME_PREFIX_LENGTH) == 0)
    {
        prim = card_menu_draw_row_text(prim, ot, CARD_MENU_TEXT_BY_OFFSET(text_table, g_card_menu_text_ring_ring_land_label), CARD_MENU_ROW_TEXT_X,
                                       y + CARD_MENU_ROW_LABEL_BASELINE, FIELD_TEXT_ALIGN_LEFT);
    }
    else if (strncmp(g_new_save_entry_prefix, name, CARD_MENU_NEW_SAVE_NAME_LENGTH) == 0)
    {
        prim = card_menu_draw_row_text(prim, ot, CARD_MENU_TEXT_BY_OFFSET(text_table, g_card_menu_text_new_save_label), CARD_MENU_ROW_TEXT_X,
                                       y + CARD_MENU_ROW_LABEL_BASELINE, FIELD_TEXT_ALIGN_LEFT);
    }
#ifdef CARD_MENU_REMASTER_EXTRA_LABEL
    else if (CARD_MENU_REMASTER_EXTRA_LABEL(name, text_table) != NULL)
    {
        prim = card_menu_draw_row_text(prim, ot, CARD_MENU_REMASTER_EXTRA_LABEL(name, text_table), CARD_MENU_ROW_TEXT_X, y + CARD_MENU_ROW_LABEL_BASELINE,
                                       FIELD_TEXT_ALIGN_LEFT);
    }
#endif
    else if (strncmp(g_lom_save_filename_prefix, name, CARD_SAVE_FILENAME_PREFIX_LENGTH) != 0)
    {
        prim = card_menu_draw_row_text(prim, ot, CARD_MENU_TEXT_BY_OFFSET(text_table, g_card_menu_text_other_game_label), CARD_MENU_ROW_TEXT_X,
                                       y + CARD_MENU_ROW_LABEL_BASELINE, FIELD_TEXT_ALIGN_LEFT);
    }

    if (row->known)
    {
        const s32 hours = row->play_time / SAVED_PLAY_TIME_TICKS_PER_HOUR;
        const s32 minutes = row->play_time / SAVED_PLAY_TIME_TICKS_PER_MINUTE - hours * SAVED_PLAY_TIME_MINUTES_PER_HOUR;

        at = 0;
        card_menu_append_number(line, &at, sizeof(line), hours);
        card_menu_append_text(line, &at, sizeof(line), (const u8*)":");
        card_menu_append_number(line, &at, sizeof(line), minutes);
        prim = card_menu_draw_row_text(prim, ot, line, CARD_MENU_ROW_TEXT_X, y + CARD_MENU_ROW_LINE1_BASELINE, FIELD_TEXT_ALIGN_LEFT);
        prim = card_menu_draw_row_text(prim, ot, CARD_MENU_TEXT(g_card_menu_location_names, row->land), CARD_MENU_ROW_PLACE_X, y + CARD_MENU_ROW_LINE1_BASELINE,
                                       FIELD_TEXT_ALIGN_LEFT);
        prim = card_menu_draw_row_text(prim, ot, (u8*)row->player, CARD_MENU_ROW_TEXT_X, y + CARD_MENU_ROW_LINE2_BASELINE, FIELD_TEXT_ALIGN_LEFT);
        prim = card_menu_draw_remaster_picture(prim, ot, row->hero == 0 ? LOM_CARDS_FACE_HERO : LOM_CARDS_FACE_HEROINE, CARD_MENU_ROW_FACE_X,
                                               y + CARD_MENU_ROW_FACE_Y, CARD_MENU_ROW_FACE_SIZE, CARD_MENU_ROW_FACE_SIZE);
    }

    return prim;
}

/**
 * @brief In the port's Remaster layout, draw the saves as the Remaster's rows: four at a time, alternately plain and
 *        shaded, the chosen one pink, each with its save's details, and arrows when there are more above or below.
 * @param prim Primitive-buffer cursor.
 * @param ot Ordering-table entry.
 * @return Advanced primitive-buffer cursor.
 * @note What each row shows is read again whenever the list is (a save can keep its name). Drawn over the whole screen,
 *       before the windows, so the rows reach the wide frame's sides and the prompts go
 *       over them. The PS1's list only shows its messages in this layout.
 */
static void* card_menu_draw_remaster_rows(void* prim, OT_TYPE* ot)
{
    const s32 listed = g_card_entry_state > 0 && g_card_entry_state < CARD_MENU_ENTRY_COUNT_LIMIT && g_card_menu_entry_scan_active == 0;
    const s32 count = listed ? g_card_entry_state : 0;
    s32 row;

    if (!lom_draw_cards_on())
    {
        return prim;
    }

    if (!listed)
    {
        for (row = 0; row < CARD_MENU_ENTRY_COUNT_LIMIT; row++)
        {
            s_card_menu_rows[row].slot = -1;
        }
    }

    if (g_card_menu_selected_row < s_card_menu_rows_top)
    {
        s_card_menu_rows_top = g_card_menu_selected_row;
    }

    if (g_card_menu_selected_row >= s_card_menu_rows_top + CARD_MENU_ROWS_SHOWN)
    {
        s_card_menu_rows_top = g_card_menu_selected_row - CARD_MENU_ROWS_SHOWN + 1;
    }

    if (s_card_menu_rows_top > count - CARD_MENU_ROWS_SHOWN)
    {
        s_card_menu_rows_top = count > CARD_MENU_ROWS_SHOWN ? count - CARD_MENU_ROWS_SHOWN : 0;
    }

    for (row = 0; row < CARD_MENU_ROWS_SHOWN; row++)
    {
        const s32 index = s_card_menu_rows_top + row;
        const s32 y = CARD_MENU_ROWS_TOP + row * CARD_MENU_ROW_HEIGHT;

        if (index < count)
        {
            prim = card_menu_draw_remaster_row(prim, ot, index, y);
        }

        if (index < count && index == g_card_menu_selected_row)
        {
            prim = card_menu_draw_remaster_picture(prim, ot, LOM_CARDS_ROW_SELECTED, CARD_MENU_ROW_LEFT, y, CARD_MENU_ROW_WIDTH, CARD_MENU_ROW_HEIGHT);
        }
        else if (index & 1)
        {
            prim = card_menu_draw_remaster_picture(prim, ot, LOM_CARDS_ROW_DARK, CARD_MENU_ROW_LEFT, y, CARD_MENU_ROW_WIDTH, CARD_MENU_ROW_HEIGHT);
        }
    }

    if (s_card_menu_rows_top > 0)
    {
        prim = card_menu_draw_remaster_picture(prim, ot, LOM_CARDS_ARROW_UP, CARD_MENU_ROW_ARROW_X, CARD_MENU_ROW_ARROW_UP_Y, CARD_MENU_ROW_ARROW_W,
                                               CARD_MENU_ROW_ARROW_H);
    }

    if (s_card_menu_rows_top + CARD_MENU_ROWS_SHOWN < count)
    {
        prim = card_menu_draw_remaster_picture(prim, ot, LOM_CARDS_ARROW_DOWN, CARD_MENU_ROW_ARROW_X, CARD_MENU_ROW_ARROW_DOWN_Y, CARD_MENU_ROW_ARROW_W,
                                               CARD_MENU_ROW_ARROW_H);
    }

    return prim;
}
