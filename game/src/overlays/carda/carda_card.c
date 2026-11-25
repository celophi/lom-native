#include "internal/carda_card.h"
#include "internal/carda_internal.h"
#include <fcntl.h>
#include "lom_native_card.h"
#include "lom_native_time.h"

#define CARDA_POCKETSTATION_SAVE_BYTES 0xC000

#define CARDA_POCKETSTATION_USED_BLOCK_LIMIT (CARD_SAVE_SLOTS + 1 - CARDA_POCKETSTATION_SAVE_BYTES / CARD_BLOCK_BYTES)

#define CARDA_CARD_TYPE_WAIT_FRAMES 120

#define CARDA_RANK_SLOTS 17

#define CARDA_SUFFIX_FLAG_COUNT 10

#define CARDA_NEW_SAVE_SUFFIX_MAX 8

#define CARDA_SAVE_PREFIX_BYTES 0x400

#define CARDA_TITLE_NOTE_OFFSET (CARD_FILE_TITLE_OFFSET + CARDA_TITLE_NOTE_INDEX)

static s32 carda_rank_entries(void);
static s32 carda_has_known_entry_type(void);
s32 carda_card_lacks_free_blocks(void);
void carda_erase_placeholder_files(void);
static s32 carda_begin_entry_scan(s32 page);
static s32 carda_scan_next_entry(s32 page);
static void carda_sort_entries_by_type(void);

#include "../../common/sjis/format_decimal.inc.c"
#include "../../common/save_file/format_hex.inc.c"
#include "../../common/save_file/hex_nibble_to_ascii.inc.c"
#include "../../common/save_file/parse_hex.inc.c"
#include "../../common/save_file/parse_hex_suffix_byte.inc.c"

#include "../../common/card_directory/parse_entry_fields.inc.c"

/**
 * @brief Sort the current card's directory, rank its saves and number the new-save entry.
 *
 * Legend of Mana saves are ranked by serial into g_card_entry_ranks, the serial a
 * new save gets goes to g_card_entry_value_limit, and the new-save placeholder gets
 * the lowest suffix (1 to 8) that no save on the card uses.
 *
 * @return Index of the save with the highest serial, or 0 when there is none.
 */
static s32 carda_rank_entries(void)
{
    s32 suffix_used[CARDA_SUFFIX_FLAG_COUNT];
    s32 entry_index;
    s32 previous_index;
    s32 higher_count;
    s32 next_rank;
    s32 maximum;
    s32 max_suffix;
    u8* serial_text;
    const char* filename_fields;
    s32 digits_left;
    s32 serial_value;

    parse_entry_fields();
    carda_sort_entries_by_type();
    for (entry_index = CARDA_SUFFIX_FLAG_COUNT - 1; entry_index >= 0; entry_index--)
    {
        suffix_used[entry_index] = 0;
    }

    max_suffix = 0;
    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        if (strncmp(g_lom_save_filename_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) == 0)
        {
            digits_left = CARD_SERIAL_DIGITS;
            serial_text = CARD_ENTRY_SERIAL_TEXT(g_card_slot, entry_index);
            serial_value = 0;
            while ((*serial_text >= '0' && *serial_text <= '9') || (*serial_text >= 'a' && *serial_text <= 'f') || (*serial_text >= 'A' && *serial_text <= 'F'))
            {
                if (digits_left == 0)
                {
                    break;
                }
                serial_value <<= CARD_SERIAL_DIGIT_BITS;
                if (*serial_text >= '0' && *serial_text <= '9')
                {
                    u32 adjusted_serial;

                    adjusted_serial = serial_value - '0';
                    serial_value = adjusted_serial + *serial_text;
                }
                else if (*serial_text >= 'A' && *serial_text <= 'F')
                {
                    u32 adjusted_serial;

                    adjusted_serial = serial_value - ('A' - CARD_SERIAL_DECIMAL_DIGITS);
                    serial_value = adjusted_serial + *serial_text;
                }
                else if (*serial_text >= 'a' && *serial_text <= 'f')
                {
                    u32 adjusted_serial;

                    adjusted_serial = serial_value - ('a' - CARD_SERIAL_DECIMAL_DIGITS);
                    serial_value = adjusted_serial + *serial_text;
                }
                serial_text++;
                digits_left--;
            }
            filename_fields = &g_card_entries[g_card_slot][entry_index].name[CARD_SAVE_FILENAME_PREFIX_LENGTH];
            g_card_entry_fields[g_card_slot][entry_index] = serial_value;
            g_card_entry_suffix_values[entry_index] = parse_hex_suffix_byte(filename_fields);
            suffix_used[g_card_entry_suffix_values[entry_index]] = 1;
            if (max_suffix < g_card_entry_suffix_values[entry_index])
            {
                max_suffix = g_card_entry_suffix_values[entry_index];
            }
        }
        else
        {
            g_card_entry_fields[g_card_slot][entry_index] = CARD_ENTRY_SERIAL_NONE;
            g_card_entry_suffix_values[entry_index] = 0;
        }
    }

    maximum = -1;
    carda_reset_entry_ranks();
    next_rank = 1;
    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        if (g_card_entry_fields[g_card_slot][entry_index] >= 0)
        {
            if (g_card_entry_fields[g_card_slot][entry_index] >= maximum)
            {
                g_card_entry_ranks[entry_index] = next_rank;
                maximum = g_card_entry_fields[g_card_slot][entry_index];
                next_rank++;
            }
            else
            {
                higher_count = 0;
                for (previous_index = 0; previous_index < entry_index; previous_index++)
                {
                    if (g_card_entry_fields[g_card_slot][entry_index] < g_card_entry_fields[g_card_slot][previous_index])
                    {
                        higher_count++;
                        g_card_entry_ranks[previous_index]++;
                    }
                }
                g_card_entry_ranks[entry_index] = next_rank - higher_count;
                next_rank++;
            }
        }
    }
    g_card_rank_count = next_rank;
    next_rank = -1;
    maximum = 0;
    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        if (next_rank < g_card_entry_fields[g_card_slot][entry_index])
        {
            next_rank = g_card_entry_fields[g_card_slot][entry_index];
            maximum = entry_index;
        }
    }
    g_card_entry_value_limit = next_rank + 1;

    for (max_suffix = 1; max_suffix <= CARDA_NEW_SAVE_SUFFIX_MAX; max_suffix++)
    {
        if (suffix_used[max_suffix] == 0)
        {
            break;
        }
    }
    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        if (strncmp(g_new_save_entry_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_MENU_NEW_SAVE_NAME_LENGTH) == 0)
        {
            g_card_entry_suffix_values[entry_index] = max_suffix;
            break;
        }
    }
    return maximum;
}

/** @brief Mark every rank slot unused and reset the rank count. */
inline void carda_reset_entry_ranks(void)
{
    s32 rank_index;
    s32 empty_rank;

    g_card_rank_count = CARD_RANK_COUNT_NONE;
    empty_rank = -1;
    for (rank_index = CARDA_RANK_SLOTS - 1; rank_index >= 0; rank_index--)
    {
        g_card_entry_ranks[rank_index] = empty_rank;
    }
}

/**
 * @brief Check whether the current card holds a file this mode can use.
 * @return 1 if an entry is a Legend of Mana save or a Ring Ring Land file (only
 *         Ring Ring Land when sending a pet), otherwise 0.
 */
static s32 carda_has_known_entry_type(void)
{
    s32 entry_index;

    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        if (g_card_menu_mode != CARDA_MODE_SEND_PET)
        {
            if (strncmp(g_lom_save_filename_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) == 0 ||
                strncmp(g_lom_pocketstation_filename_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) == 0)
            {
                return 1;
            }
        }
        if (g_card_menu_mode == CARDA_MODE_SEND_PET &&
            strncmp(g_lom_pocketstation_filename_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) == 0)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Check whether the current card lacks room for a new file.
 * @return 1 if the used blocks leave no room for the mode's file, otherwise 0.
 * @note Sending a pet writes the six-block Ring Ring Land file; every other mode
 *       writes a two-block save.
 */
inline s32 carda_card_lacks_free_blocks(void)
{
    s32 entry_index;
    s32 used_blocks;

    used_blocks = 0;
    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        used_blocks += g_card_entries[g_card_slot][entry_index].size / CARD_BLOCK_BYTES;
    }
    if ((used_blocks < CARD_USED_BLOCK_LIMIT && g_card_menu_mode != CARDA_MODE_SEND_PET) ||
        (used_blocks < CARDA_POCKETSTATION_USED_BLOCK_LIMIT && g_card_menu_mode == CARDA_MODE_SEND_PET))
    {
        return 0;
    }
    return 1;
}

/**
 * @brief Erase the save and Ring Ring Land placeholder files left by an interrupted
 *        write, then clear pending card events.
 */
inline void carda_erase_placeholder_files(void)
{
    CardFilePath card_path;

    lom_native_niki_string_copy(card_path.text, CARD_DEVICE_PREFIX);
    card_path.device.characters.slot += g_card_slot;
    lom_native_niki_concat(card_path.text, g_lom_save_dummy_filename);
    lom_native_card_erase_libapi(card_path.text);

    lom_native_niki_string_copy(card_path.text, CARD_DEVICE_PREFIX);
    card_path.device.characters.slot += g_card_slot;
    lom_native_niki_concat(card_path.text, g_lom_pocketstation_dummy_filename);
    lom_native_card_erase_libapi(card_path.text);
    clear_software_card_events();
    clear_hardware_card_events();
}

/**
 * @brief Run the current step of the card sequence and advance g_card_step.
 * @return CardMenuSequenceResult for the caller.
 * @note g_card_step walks one of the g_card_steps_* or g_carda_steps_* byte tables.
 *       Each CardMenuStep or CardaCardStep opcode issues or polls a card command,
 *       reads or writes a save, or scans the card directory. New files are written
 *       under a placeholder name first and renamed once the write has finished.
 *       Opcodes without a case make the sequence wait on them.
 */
s32 carda_advance_card_sequence(void)
{
    CardFilePath card_path;
    CardFilePath file_name;
    CardFilePath device_path;
    u8 title_frame[CARD_SECTOR_BYTES];
    struct DIRENTRY dir_entry;
    s32 attempts;
    s32 check_attempts;
    s32 poll_result;
    s32 phase_result;
    long command;
    long result;
    u8* title_src;
    u8* title_dst;

    lom_native_niki_string_copy(card_path.text, CARD_DEVICE_PREFIX);
    card_path.device.characters.slot += g_card_slot;
    lom_native_niki_string_copy(device_path.text, card_path.text);
    phase_result = CARD_MENU_SEQUENCE_WAIT;
    if (CARDA_IS_POCKETSTATION_MODE(g_card_menu_mode))
    {
        if (g_card_step == g_card_steps_initial_scan)
        {
            g_card_step = g_carda_steps_initial_scan_check_type;
        }
    }
    if (g_card_step != NULL)
    {
        switch (*g_card_step)
        {
        case CARD_MENU_STEP_CARD_INFO:
            phase_result = CARD_MENU_SEQUENCE_RUN_AGAIN;
            lom_native_card_wait(g_card_slot);
            lom_native_card_info(CARD_CHANNEL(g_card_slot));
            g_card_step++;
            break;

        case CARD_MENU_STEP_POLL_CARD_INFO:
            poll_result = poll_software_card_events();
            switch (poll_result)
            {
            case CARD_EVENT_COMPLETE:
                g_card_step++;
                break;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
                phase_result = CARD_MENU_SEQUENCE_NO_CARD;
                g_card_menu_selection_status = CARD_MENU_SELECTION_NONE;
                g_card_entry_state = CARD_MENU_ENTRY_STATE_NO_CARD;
                g_card_step++;
                card_menu_deactivate_primary_element();
                break;
            case CARD_EVENT_NEW_CARD:
                carda_reset_entry_ranks();
                g_card_entry_state = CARD_MENU_ENTRY_STATE_CHECKING_CARD;
                g_card_step = g_card_steps_initial_scan;
                break;
            }
            break;

        case CARDA_STEP_POLL_CARD_PRESENT:
            switch (poll_software_card_events())
            {
            case CARD_EVENT_COMPLETE:
                g_card_step++;
                break;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
            case CARD_EVENT_NEW_CARD:
                carda_open_status_dialog(CARDA_DIALOG_FORMAT_FAILED);
                break;
            }
            break;

        case CARD_MENU_STEP_CLEAR_SOFTWARE_EVENTS:
            clear_software_card_events();
            g_card_step++;
            break;

        case CARD_MENU_STEP_POLL_HARDWARE_EVENTS:
            while ((poll_result = poll_hardware_card_events()) == CARD_EVENT_NONE);
            switch (poll_result)
            {
            case CARD_EVENT_COMPLETE:
                g_card_step++;
                break;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
            case CARD_EVENT_NEW_CARD:
                phase_result = CARD_MENU_SEQUENCE_NO_CARD;
                g_card_menu_selection_status = CARD_MENU_SELECTION_NONE;
                g_card_entry_state = CARD_MENU_ENTRY_STATE_NO_CARD;
                break;
            }
            break;

        case CARD_MENU_STEP_CLEAR_HARDWARE_EVENTS:
            clear_hardware_card_events();
            g_card_step++;
            break;

        case CARD_MENU_STEP_SCAN_ENTRIES:
            carda_erase_placeholder_files();
            g_card_menu_entry_scan_active = 1;
            if (carda_begin_entry_scan(g_card_slot) == 0)
            {
                phase_result = CARD_MENU_SEQUENCE_FINISHED;
                g_card_entry_state = CARD_MENU_ENTRY_STATE_NO_GAME_DATA;
                g_card_step = NULL;
                g_card_menu_entry_scan_active = 0;
                break;
            }
            g_card_step++;
            for (attempts = 0; attempts < CARD_MENU_FILE_OP_ATTEMPTS; attempts++)
            {
                if (carda_scan_next_entry(g_card_slot) == 0)
                {
                    g_card_menu_entry_scan_active = 0;
                    if (g_card_entry_state != CARD_MENU_ENTRY_STATE_NO_GAME_DATA && g_card_entry_state != CARD_MENU_ENTRY_STATE_CARD_FULL)
                    {
                        if (g_card_entry_state != CARDA_ENTRY_STATE_NO_ROOM_FOR_DOWNLOAD)
                        {
                            carda_commit_selected_entry();
                        }
                    }
                    break;
                }
            }
            break;

        case CARD_MENU_STEP_CLEAR_CARD:
            phase_result = CARD_MENU_SEQUENCE_RUN_AGAIN;
            lom_native_card_wait(g_card_slot);
            lom_native_card_clear(CARD_CHANNEL(g_card_slot));
            g_card_step++;
            break;

        case CARD_MENU_STEP_LOAD_CARD:
            phase_result = CARD_MENU_SEQUENCE_RUN_AGAIN;
            lom_native_card_wait(g_card_slot);
            lom_native_card_load(CARD_CHANNEL(g_card_slot));
            g_card_menu_new_card_retries = CARD_MENU_CARD_LOAD_RETRIES;
            g_card_menu_load_error_retries = CARD_MENU_CARD_LOAD_RETRIES;
            g_card_step++;
            break;

        case CARD_MENU_STEP_DONE:
            phase_result = CARD_MENU_SEQUENCE_FINISHED;
            g_card_menu_write_in_progress = 0;
            break;

        case CARDA_STEP_SKIP:
            g_card_step++;
            break;

        case CARDA_STEP_CREATE_TEMP_SAVE:
            if (g_card_menu_has_free_space == 0)
            {
                lom_native_niki_concat(card_path.text, g_card_entries[g_card_slot][g_card_menu_selected_row].name);
                CARD_ERASE_WITH_RETRIES(attempts, card_path.text);
                clear_software_card_events();
                clear_hardware_card_events();
                lom_native_niki_string_copy(card_path.text, device_path.text);
            }
            lom_native_niki_string_copy(file_name.text, g_lom_save_dummy_filename);
            lom_native_niki_concat(card_path.text, file_name.text);
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(card_path.text, FCREAT | CARD_FILE_BLOCKS(SAVE_FILE_BYTES / CARD_BLOCK_BYTES));
            if (g_card_menu_file_handle == CARD_FILE_ERROR)
            {
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count == 0)
                {
                    clear_software_card_events();
                    clear_hardware_card_events();
                    carda_open_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                    return phase_result;
                }
                break;
            }
            lom_native_card_close_libapi(g_card_menu_file_handle);
            lom_native_niki_string_copy(g_card_temp_save_path, card_path.text);
            g_card_step++;
            break;

        case CARDA_STEP_WRITE_TEMP_SAVE:
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(g_card_temp_save_path, FASYNC | FWRITE);
            clear_software_card_events();
            g_card_menu_progress_bar_active = 1;
            g_card_menu_progress_start_tick = lom_time_frames();
            lom_native_card_wait(g_card_slot);
            if (lom_native_card_write_libapi(g_card_menu_file_handle, g_carda_save_blob, SAVE_FILE_BYTES) == CARD_FILE_ERROR)
            {
                lom_native_card_close_libapi(g_card_menu_file_handle);
                CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count != 0)
                {
                    g_card_step--;
                    break;
                }
                clear_software_card_events();
                clear_hardware_card_events();
                carda_open_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                return phase_result;
            }
            g_card_step++;
            break;

        case CARDA_STEP_POLL_TEMP_SAVE_WRITE:
            switch (poll_software_card_events())
            {
            case CARD_EVENT_COMPLETE:
                if (g_card_menu_has_free_space != 0)
                {
                    lom_native_niki_string_copy(card_path.text, device_path.text);
                    lom_native_niki_concat(card_path.text, g_card_entries[g_card_slot][g_card_menu_selected_row].name);
                    lom_native_card_wait(g_card_slot);
                    CARD_ERASE_WITH_RETRIES(attempts, card_path.text);
                    clear_software_card_events();
                    clear_hardware_card_events();
                }

                lom_native_niki_string_copy(card_path.text, device_path.text);
                lom_native_niki_string_copy(file_name.text, g_lom_save_filename_prefix);
                format_hex(&file_name.text[CARD_SAVE_FILENAME_PREFIX_LENGTH], g_card_entry_value_limit, CARD_SERIAL_DIGITS);
                lom_native_niki_concat(card_path.text, file_name.text);
                if (carda_is_cleared_game_save() != 0)
                {
                    file_name.text[0] = '+';
                }
                else
                {
                    file_name.text[0] = '-';
                }
                file_name.text[1] = g_card_entry_suffix_values[g_card_menu_selected_row] + '0';
                file_name.text[2] = 0;
                lom_native_niki_concat(card_path.text, file_name.text);

                lom_native_card_wait(g_card_slot);
                for (attempts = 0; attempts < CARD_MENU_FILE_OP_ATTEMPTS; attempts++)
                {
                    if (lom_native_card_rename_libapi(g_card_temp_save_path, card_path.text) != 0)
                    {
                        break;
                    }
                }

                for (attempts = 0; attempts < CARD_MENU_FILE_OP_ATTEMPTS; attempts++)
                {
                    if (lom_native_niki_first(card_path.text, &dir_entry) != 0)
                    {
                        break;
                    }
                }
                if (attempts != CARD_MENU_FILE_OP_ATTEMPTS)
                {
                    lom_native_card_wait(g_card_slot);
                    lom_native_card_read_sector(CARD_CHANNEL(g_card_slot), dir_entry.head, title_frame);
                    lom_native_card_wait(g_card_slot);
                    title_dst = &title_frame[CARD_FILE_TITLE_OFFSET];
                    title_src = g_carda_save_title_template;
                    attempts = 0;
                    while (attempts < CARDA_TITLE_TEMPLATE_BYTES)
                    {
                        attempts++;
                        *title_dst++ = *title_src++;
                    }
                    if (carda_is_cleared_game_save() != 0)
                    {
                        title_frame[CARDA_TITLE_NOTE_OFFSET] = SJIS_NOTE_SIGN >> SJIS_CODE_LEAD_SHIFT;
                        title_frame[CARDA_TITLE_NOTE_OFFSET + 1] = SJIS_NOTE_SIGN & SJIS_CODE_TRAIL_MASK;
                    }
                    lom_native_card_wait(g_card_slot);
                    lom_native_card_write_sector(CARD_CHANNEL(g_card_slot), dir_entry.head, title_frame);
                    lom_native_card_wait(g_card_slot);
                }
                g_card_menu_write_in_progress = 0;
                g_card_step++;
                lom_native_card_close_libapi(g_card_menu_file_handle);
                g_card_entry_state = CARD_MENU_ENTRY_STATE_CHECKING_CARD;
                g_card_step = g_card_steps_initial_scan;
                break;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
            case CARD_EVENT_NEW_CARD:
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count != 0)
                {
                    lom_native_card_close_libapi(g_card_menu_file_handle);
                    CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                    g_card_step -= 2;
                }
                else
                {
                    lom_native_card_close_libapi(g_card_menu_file_handle);
                    CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                    g_card_menu_progress_bar_active = 0;
                    g_card_menu_progress_start_tick = lom_time_frames();
                    clear_software_card_events();
                    clear_hardware_card_events();
                    carda_open_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                    return phase_result;
                }
                break;
            }
            break;

        case CARDA_STEP_CREATE_POCKETSTATION_SAVE:
            g_card_menu_progress_bar_active = 1;
            g_card_menu_progress_start_tick = lom_time_frames();
            if (strncmp(g_new_save_entry_prefix, g_card_entries[g_card_slot][g_card_menu_selected_row].name, CARD_MENU_NEW_SAVE_NAME_LENGTH) != 0)
            {
                lom_native_niki_concat(card_path.text, g_card_entries[g_card_slot][g_card_menu_selected_row].name);
                CARD_ERASE_WITH_RETRIES(attempts, card_path.text);
                clear_software_card_events();
                clear_hardware_card_events();
            }
            lom_native_niki_string_copy(card_path.text, device_path.text);
            lom_native_niki_concat(card_path.text, g_lom_pocketstation_filename_prefix);
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(card_path.text, FCREAT | CARD_FILE_BLOCKS(CARDA_POCKETSTATION_SAVE_BYTES / CARD_BLOCK_BYTES));
            if (g_card_menu_file_handle == CARD_FILE_ERROR)
            {
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count == 0)
                {
                    lom_native_card_close_libapi(g_card_menu_file_handle);
                    carda_open_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                    return phase_result;
                }
                break;
            }
            lom_native_card_close_libapi(g_card_menu_file_handle);
            lom_native_niki_string_copy(g_card_temp_save_path, card_path.text);
            g_card_step++;
            break;

        case CARDA_STEP_WRITE_POCKETSTATION_SAVE:
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(g_card_temp_save_path, FASYNC | FWRITE);
            lom_native_card_wait(g_card_slot);
            if (lom_native_card_write_libapi(g_card_menu_file_handle, g_carda_save_blob, CARDA_POCKETSTATION_SAVE_BYTES) == CARD_FILE_ERROR)
            {
                lom_native_card_mcx_hide_trans(CARD_CHANNEL(g_card_slot));
                lom_native_card_close_libapi(g_card_menu_file_handle);
                CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count != 0)
                {
                    g_card_step--;
                    break;
                }
                clear_software_card_events();
                clear_hardware_card_events();
                carda_open_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                return phase_result;
            }
            g_card_step++;
            break;

        case CARDA_STEP_POLL_POCKETSTATION_SAVE_WRITE:
            switch (poll_software_card_events())
            {
            case CARD_EVENT_COMPLETE:
                g_card_menu_write_in_progress = 0;
                g_card_step++;
                lom_native_card_close_libapi(g_card_menu_file_handle);
                g_card_step = g_card_steps_initial_scan;
                for (attempts = 0; attempts < CARD_MENU_FILE_OP_ATTEMPTS; attempts++)
                {
                    if (lom_native_niki_first(g_card_temp_save_path, &dir_entry) != 0)
                    {
                        break;
                    }
                }
                if (attempts == CARD_MENU_FILE_OP_ATTEMPTS)
                {
                    CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                    clear_software_card_events();
                    clear_hardware_card_events();
                    carda_open_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                    return phase_result;
                }
                for (attempts = 0; attempts < CARD_MENU_FILE_OP_ATTEMPTS; attempts++)
                {
                    if (lom_native_card_mcx_exec_flag(CARD_CHANNEL(g_card_slot), dir_entry.head / CARD_BLOCK_SECTORS, 1) != 0)
                    {
                        break;
                    }
                }
                if (attempts != CARD_MENU_FILE_OP_ATTEMPTS)
                {
                    return phase_result;
                }
                CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                clear_software_card_events();
                clear_hardware_card_events();
                carda_open_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                return phase_result;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
            case CARD_EVENT_NEW_CARD:
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count != 0)
                {
                    lom_native_card_close_libapi(g_card_menu_file_handle);
                    CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                    g_card_step -= 2;
                }
                else
                {
                    lom_native_card_close_libapi(g_card_menu_file_handle);
                    CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                    clear_software_card_events();
                    clear_hardware_card_events();
                    carda_open_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                    g_card_menu_progress_bar_active = 0;
                    g_card_menu_progress_start_tick = lom_time_frames();
                }
                break;
            }
            break;

        case CARD_MENU_STEP_POLL_CARD_LOAD:
            switch (poll_software_card_events())
            {
            case CARD_EVENT_COMPLETE:
                g_card_step++;
                break;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
                g_card_menu_load_error_retries--;
                if (g_card_menu_load_error_retries != 0)
                {
                    lom_native_card_wait(g_card_slot);
                    lom_native_card_clear(CARD_CHANNEL(g_card_slot));
                    lom_native_card_wait(g_card_slot);
                    clear_software_card_events();
                    lom_native_card_load(CARD_CHANNEL(g_card_slot));
                    break;
                }
                phase_result = CARD_MENU_SEQUENCE_NO_CARD;
                g_card_menu_selection_status = CARD_MENU_SELECTION_NONE;
                g_card_entry_state = CARD_MENU_ENTRY_STATE_NO_CARD;
                break;
            case CARD_EVENT_NEW_CARD:
                g_card_menu_new_card_retries--;
                if (g_card_menu_new_card_retries != 0)
                {
                    lom_native_card_wait(g_card_slot);
                    lom_native_card_clear(CARD_CHANNEL(g_card_slot));
                    lom_native_card_wait(g_card_slot);
                    clear_software_card_events();
                    lom_native_card_load(CARD_CHANNEL(g_card_slot));
                    break;
                }
                phase_result = CARD_MENU_SEQUENCE_UNFORMATTED;
                g_card_entry_state = CARD_MENU_ENTRY_STATE_NO_SAVE_DATA;
                g_card_step = g_card_steps_idle;
                break;
            }
            break;

        case CARD_MENU_STEP_WAIT_HARDWARE_EVENTS:
            lom_native_card_wait(g_card_slot);
            g_card_step++;
            break;

        case CARD_MENU_STEP_READ_ENTRY:
            g_card_menu_io_busy = 1;
            g_card_menu_selection_status = CARD_MENU_SELECTION_NONE;
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(g_card_selected_save_path, FASYNC | FREAD);
            if (g_card_menu_file_handle != CARD_FILE_ERROR)
            {
                clear_software_card_events();
                lom_native_card_wait(g_card_slot);
                if (lom_native_card_read_libapi(g_card_menu_file_handle, &g_carda_selected_file,
                                                g_card_menu_selected_entry_extended != 0 ? CARD_MENU_ENTRY_READ_BYTES : CARD_MENU_ENTRY_TITLE_READ_BYTES) ==
                    CARD_FILE_ERROR)
                {
                    lom_native_card_close_libapi(g_card_menu_file_handle);
                    return phase_result;
                }
                g_card_step++;
            }
            break;

        case CARD_MENU_STEP_POLL_ENTRY_READ:
            poll_result = poll_software_card_events();
            if (poll_result == CARD_EVENT_COMPLETE)
            {
                g_card_menu_io_busy = 0;
                g_card_menu_selection_status = CARD_MENU_SELECTION_ENTRY_READ;
                g_card_step++;
                lom_native_card_close_libapi(g_card_menu_file_handle);
            }
            else if (poll_result != CARD_EVENT_NONE)
            {
                g_card_entry_state = CARD_MENU_ENTRY_STATE_CHECKING_CARD;
                g_card_menu_io_busy = 0;
                lom_native_card_close_libapi(g_card_menu_file_handle);
                g_card_step = g_card_steps_initial_scan;
            }
            break;

        case CARD_MENU_STEP_READ_SAVE:
            g_card_menu_progress_active = 1;
            g_card_menu_progress_start_tick = lom_time_frames();
            g_card_menu_progress_bar_active = 1;
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(g_card_selected_save_path, FASYNC | FREAD);
            clear_software_card_events();
            lom_native_card_wait(g_card_slot);
            if (lom_native_card_read_libapi(g_card_menu_file_handle, g_carda_save_blob, SAVE_FILE_BYTES) == CARD_FILE_ERROR)
            {
                carda_open_status_dialog(CARD_MENU_DIALOG_LOAD_FAILED);
                return phase_result;
            }
            g_card_step++;
            break;

        case CARD_MENU_STEP_POLL_SAVE_READ:
            switch (poll_software_card_events())
            {
            case CARD_EVENT_COMPLETE:
                g_card_menu_progress_active = 0;
                g_card_step++;
                lom_native_card_close_libapi(g_card_menu_file_handle);
                break;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
            case CARD_EVENT_NEW_CARD:
                g_card_menu_progress_start_tick = lom_time_frames();
                g_card_menu_progress_bar_active = 0;
                carda_open_status_dialog(CARD_MENU_DIALOG_LOAD_FAILED);
                break;
            }
            break;

        case CARD_MENU_STEP_CHECK_POCKETSTATION:
            for (check_attempts = 0; check_attempts < CARD_MENU_FILE_OP_ATTEMPTS; check_attempts++)
            {
                for (attempts = 0; attempts < CARDA_CARD_TYPE_WAIT_FRAMES; attempts++)
                {
                    if (lom_native_niki_card_type(CARD_CHANNEL(g_card_slot)) == MCX_COMMAND_ISSUED)
                    {
                        break;
                    }
                    lom_time_wait_menu(0);
                }
                lom_time_wait_menu(0);
                if (attempts != CARD_MENU_FILE_OP_ATTEMPTS)
                {
                    lom_native_card_mcx_sync(MCX_SYNC_WAIT, &command, &result);
                    switch (result)
                    {
                    case McxErrSuccess:
                        g_card_step++;
                        return phase_result;
                    case McxErrNoCard:
                        g_card_entry_state = CARD_MENU_ENTRY_STATE_NO_CARD;
                        g_card_step = NULL;
                        return phase_result;
                    }
                }
            }
            if (g_card_entry_state != CARDA_ENTRY_STATE_NOT_POCKETSTATION)
            {
                g_card_entry_state = CARDA_ENTRY_STATE_NOT_POCKETSTATION;
            }
            g_card_step = NULL;
            return phase_result;

        case CARDA_STEP_READ_SAVE_PREFIX:
            g_card_menu_progress_active = 1;
            g_card_menu_progress_start_tick = lom_time_frames();
            g_card_menu_progress_bar_active = 1;
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(g_card_selected_save_path, FASYNC | FREAD);
            clear_software_card_events();
            lom_native_card_wait(g_card_slot);
            if (lom_native_card_read_libapi(g_card_menu_file_handle, g_carda_save_blob, CARDA_SAVE_PREFIX_BYTES) == CARD_FILE_ERROR)
            {
                carda_open_save_status_dialog(CARD_MENU_DIALOG_LOAD_FAILED);
                return phase_result;
            }
            g_card_step++;
            break;

        case CARDA_STEP_POLL_SAVE_PREFIX_READ:
            switch (poll_software_card_events())
            {
            case CARD_EVENT_COMPLETE:
                g_card_menu_progress_active = 0;
                g_card_step++;
                lom_native_card_close_libapi(g_card_menu_file_handle);
                break;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
            case CARD_EVENT_NEW_CARD:
                g_card_menu_progress_bar_active = 0;
                carda_open_save_status_dialog(CARD_MENU_DIALOG_LOAD_FAILED);
                return phase_result;
            }
            break;

        case CARD_MENU_STEP_INIT_RETRIES:
            g_card_menu_retry_count = CARD_MENU_SAVE_RETRIES;
            g_card_step++;
            break;

        case CARDA_STEP_WRITE_TEMP_POCKETSTATION_SAVE:
            if (g_card_menu_has_free_space == 0)
            {
                CARD_ERASE_WITH_RETRIES(attempts, g_card_selected_save_path);
            }
            lom_native_niki_concat(card_path.text, g_lom_pocketstation_dummy_filename);
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(card_path.text, FCREAT | CARD_FILE_BLOCKS(CARDA_POCKETSTATION_SAVE_BYTES / CARD_BLOCK_BYTES));
            if (g_card_menu_file_handle == CARD_FILE_ERROR)
            {
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count == 0)
                {
                    lom_native_card_close_libapi(g_card_menu_file_handle);
                    CARD_ERASE_WITH_RETRIES(attempts, card_path.text);
                    carda_open_save_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                    return phase_result;
                }
                break;
            }
            lom_native_card_close_libapi(g_card_menu_file_handle);
            lom_native_niki_string_copy(g_card_temp_save_path, card_path.text);
            lom_native_card_wait(g_card_slot);
            g_card_menu_file_handle = lom_native_card_open_libapi(g_card_temp_save_path, FASYNC | FWRITE);
            clear_software_card_events();
            g_card_menu_progress_start_tick = lom_time_frames();
            g_card_menu_progress_bar_active = 1;
            lom_native_card_wait(g_card_slot);
            if (lom_native_card_write_libapi(g_card_menu_file_handle, g_carda_save_blob, CARDA_POCKETSTATION_SAVE_BYTES) == CARD_FILE_ERROR)
            {
                lom_native_card_close_libapi(g_card_menu_file_handle);
                CARD_ERASE_WITH_RETRIES(attempts, g_card_temp_save_path);
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count == 0)
                {
                    carda_open_save_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                    return phase_result;
                }
                break;
            }
            g_card_step++;
            break;

        case CARDA_STEP_POLL_TEMP_POCKETSTATION_SAVE_WRITE:
            switch (poll_software_card_events())
            {
            case CARD_EVENT_COMPLETE:
                if (g_card_menu_has_free_space != 0)
                {
                    lom_native_card_wait(g_card_slot);
                    CARD_ERASE_WITH_RETRIES(attempts, g_card_selected_save_path);
                }
                lom_native_card_wait(g_card_slot);
                for (attempts = 0; attempts < CARD_MENU_FILE_OP_ATTEMPTS; attempts++)
                {
                    if (lom_native_card_rename_libapi(g_card_temp_save_path, g_card_selected_save_path) != 0)
                    {
                        break;
                    }
                }
                g_card_menu_write_in_progress = 0;
                g_card_step++;
                lom_native_card_close_libapi(g_card_menu_file_handle);
                g_card_step = g_card_steps_initial_scan;
                break;
            case CARD_EVENT_ERROR:
            case CARD_EVENT_TIMEOUT:
            case CARD_EVENT_NEW_CARD:
                g_card_menu_retry_count--;
                if (g_card_menu_retry_count == 0)
                {
                    lom_native_card_close_libapi(g_card_menu_file_handle);
                    g_card_menu_progress_bar_active = 0;
                    carda_open_save_status_dialog(CARD_MENU_DIALOG_SAVE_FAILED);
                    return phase_result;
                }
                lom_native_card_close_libapi(g_card_menu_file_handle);
                g_card_step--;
                break;
            }
            break;
        }
    }
    return phase_result;
}

/**
 * @brief Format the current card and reset the list to a single new-save entry.
 */
void carda_format_card(void)
{
    s32 unused[4];
    s32 attempt;

    for (attempt = 0; attempt < CARD_MENU_FILE_OP_ATTEMPTS; attempt++)
    {
        if (lom_native_card_format(CARD_CHANNEL(g_card_slot)) != 0)
        {
            break;
        }
    }

    g_card_menu_has_free_space = 0;
    g_card_menu_selected_row = 0;
    lom_native_niki_string_copy(g_card_entries[g_card_slot][0].name, g_new_save_entry_prefix);
}

#include "../../common/card_events/restart_card_sequence.inc.c"
#include "../../common/card_events/poll_and_retry_card_info.inc.c"

/**
 * @brief Open and enable the software and hardware memory-card events.
 */
void carda_init_card_events(void)
{
    reset_controller_vsync_state();
    lom_native_card_lock();
    g_card_software_event_io_complete = lom_native_card_open_event_libapi(SwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_card_software_event_error = lom_native_card_open_event_libapi(SwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_card_software_event_timeout = lom_native_card_open_event_libapi(SwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_card_software_event_new_card = lom_native_card_open_event_libapi(SwCARD, EvSpNEW, EvMdNOINTR, 0);
    g_card_hardware_event_io_complete = lom_native_card_open_event_libapi(HwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_card_hardware_event_error = lom_native_card_open_event_libapi(HwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_card_hardware_event_timeout = lom_native_card_open_event_libapi(HwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_card_hardware_event_new_card = lom_native_card_open_event_libapi(HwCARD, EvSpNEW, EvMdNOINTR, 0);
    lom_native_card_enable_event(g_card_software_event_io_complete);
    lom_native_card_enable_event(g_card_software_event_error);
    lom_native_card_enable_event(g_card_software_event_timeout);
    lom_native_card_enable_event(g_card_software_event_new_card);
    lom_native_card_enable_event(g_card_hardware_event_io_complete);
    lom_native_card_enable_event(g_card_hardware_event_error);
    lom_native_card_enable_event(g_card_hardware_event_timeout);
    lom_native_card_enable_event(g_card_hardware_event_new_card);
    lom_native_card_unlock();
    g_card_menu_progress_start_tick = lom_time_frames();
    g_card_menu_progress_bar_active = 0;
    g_card_menu_entry_scan_active = 0;
}

#include "../../common/card_events/shutdown_card_events.inc.c"

/**
 * @brief Reset the list view and read the first directory entry of a card.
 * @param page Memory-card slot to scan.
 * @return 1 if the first entry was read or the mode allows an empty card, 0 otherwise.
 */
static s32 carda_begin_entry_scan(s32 page)
{
    CardSearchPattern search_path;
    s32 attempt;

    lom_native_niki_string_copy(search_path.text, CARD_SEARCH_PATTERN);
    g_card_menu_scroll_frames = 0;
    g_card_menu_scroll_target_y = 0;
    g_card_menu_scroll_y = 0;
    g_card_menu_selected_row = 0;
    search_path.device.characters.slot += page;
    lom_native_card_wait(page);
    g_card_entry_state = 0;
    for (attempt = 0; attempt < CARD_MENU_FILE_OP_ATTEMPTS; attempt++)
    {
        if (lom_native_niki_first(search_path.text, g_card_entries[page]) != 0)
        {
            field_flag_known_save(g_card_entries[page][g_card_entry_state].name);
            g_card_entry_state++;
            return 1;
        }
    }
    if (g_card_menu_mode == CARDA_MODE_LOAD || g_card_menu_mode == CARDA_MODE_RETURN_PET)
    {
        return 0;
    }
    return 1;
}

/**
 * @brief Read the next directory entry, or finish and rank the completed directory.
 * @param page Memory-card slot being scanned.
 * @return 1 if another directory entry was read, otherwise 0.
 * @note When the directory is complete, a placeholder entry is appended (full card
 *       or new save, by mode), the entries are ranked and the selection is set.
 */
static s32 carda_scan_next_entry(s32 page)
{
    s32 entry_attempt;
    s32 selected;

    lom_native_card_wait(page);
    for (entry_attempt = 0; entry_attempt < CARD_MENU_FILE_OP_ATTEMPTS; entry_attempt++)
    {
        if (lom_native_niki_next(&g_card_entries[page][g_card_entry_state]) != 0)
        {
            field_flag_known_save(g_card_entries[page][g_card_entry_state].name);
            g_card_entry_state++;
            return 1;
        }
    }

    field_reset_input_repeat();
    if (g_card_menu_mode == CARDA_MODE_LOAD && carda_has_known_entry_type() == 0)
    {
        g_card_entry_state = CARD_MENU_ENTRY_STATE_NO_GAME_DATA;
    }
    else
    {
        g_card_menu_has_free_space = 0;
        if (carda_card_lacks_free_blocks())
        {
            if (g_card_menu_mode == CARDA_MODE_SAVE || g_card_menu_mode == CARDA_MODE_SEND_PET)
            {
                lom_native_niki_string_copy(g_card_entries[page][g_card_entry_state].name, g_card_full_entry_name);
                g_card_entries[page][g_card_entry_state].size = 0;
                g_card_entry_state++;
            }
            selected = carda_rank_entries();
            if (carda_has_known_entry_type() == 0)
            {
                if (CARDA_IS_POCKETSTATION_MODE(g_card_menu_mode))
                {
                    g_card_entry_state = CARDA_ENTRY_STATE_NO_ROOM_FOR_DOWNLOAD;
                }
                else
                {
                    g_card_entry_state = CARD_MENU_ENTRY_STATE_CARD_FULL;
                }
                g_card_entry_value_limit = 0;
            }
            else
            {
                g_card_menu_selected_row = selected;
                card_menu_scroll_to_selection();
            }
        }
        else
        {
            g_card_menu_has_free_space = 1;
            if (CARDA_IS_POCKETSTATION_MODE(g_card_menu_mode))
            {
                lom_native_niki_string_copy(g_card_entries[page][g_card_entry_state].name, g_new_save_entry_prefix);
                g_card_entries[page][g_card_entry_state].size = CARDA_POCKETSTATION_SAVE_BYTES;
                g_card_entry_state++;
            }
            else if (g_card_menu_mode == CARDA_MODE_SAVE)
            {
                lom_native_niki_string_copy(g_card_entries[page][g_card_entry_state].name, g_new_save_entry_prefix);
                g_card_entries[page][g_card_entry_state].size = SAVE_FILE_BYTES;
                g_card_entry_state++;
            }
            selected = carda_rank_entries();
            if (carda_has_known_entry_type() == 0)
            {
                g_card_menu_selected_row = 0;
                card_menu_scroll_to_selection();
                g_card_entry_value_limit = 0;
            }
            else
            {
                g_card_menu_selected_row = selected;
                card_menu_scroll_to_selection();
            }
        }
    }
    return 0;
}

/**
 * @brief Commit the selected entry and start reading its file header.
 * @note An empty card, the new-save placeholder and the full-card placeholder only
 *       set g_card_menu_selection_status. Any other entry gets its path built into
 *       g_card_selected_save_path and the g_card_steps_read_selected_header table armed.
 */
void carda_commit_selected_entry(void)
{
    CardEntryPath card_path;

    if (g_card_entry_state == 0)
    {
        g_card_menu_selection_status = CARD_MENU_SELECTION_EMPTY_CARD;
        return;
    }
    if (strncmp(g_new_save_entry_prefix, g_card_entries[g_card_slot][g_card_menu_selected_row].name, CARD_MENU_NEW_SAVE_NAME_LENGTH) == 0)
    {
        g_card_menu_selection_status = CARD_MENU_SELECTION_NEW_SAVE;
        return;
    }
    if (strncmp(g_card_full_entry_name, g_card_entries[g_card_slot][g_card_menu_selected_row].name, CARDA_CARD_FULL_ENTRY_NAME_LENGTH) == 0)
    {
        g_card_menu_selection_status = CARDA_SELECTION_CARD_FULL;
        return;
    }
    lom_native_niki_string_copy(card_path.text, CARD_DEVICE_PREFIX);
    lom_native_niki_concat(card_path.text, g_card_entries[g_card_slot][g_card_menu_selected_row].name);
    card_path.device.characters.slot += g_card_slot;
    g_card_menu_selection_status = CARD_MENU_SELECTION_NONE;
    lom_native_niki_string_copy(g_card_selected_save_path, card_path.text);
    g_card_step = g_card_steps_read_selected_header;
    g_card_menu_io_busy = 1;
    if (strncmp(g_lom_save_filename_prefix, g_card_entries[g_card_slot][g_card_menu_selected_row].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) == 0)
    {
        g_card_menu_selected_entry_extended = 1;
    }
    else
    {
        g_card_menu_selected_entry_extended = 0;
    }
}

#include "../../common/card_events/clear_software_card_events.inc.c"
#include "../../common/card_events/clear_hardware_card_events.inc.c"
#include "../../common/card_events/poll_software_card_events.inc.c"
#include "../../common/card_events/poll_hardware_card_events.inc.c"

/**
 * @brief Reorder the current card's directory by save type and suffix.
 * @note Legend of Mana saves come first, then Ring Ring Land files (each grouped by
 *       suffix), then the new-save and full-card placeholders, then every other file.
 */
static void carda_sort_entries_by_type(void)
{
    struct DIRENTRY sorted_entries[CARD_DIRECTORY_ENTRY_COUNT];
    s32 output_count = 0;
    s32 group;
    s32 entry_index;

    for (group = 0; group < CARD_MENU_ENTRY_GROUP_COUNT; group++)
    {
        for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
        {
            if (g_card_entry_suffix_values[entry_index] == group &&
                strncmp(g_lom_save_filename_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) == 0)
            {
                lom_native_save_copy((u8*)&g_card_entries[g_card_slot][entry_index], (u8*)&sorted_entries[output_count], sizeof(struct DIRENTRY));
                output_count++;
            }
        }
    }

    for (group = 0; group < CARD_MENU_ENTRY_GROUP_COUNT; group++)
    {
        for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
        {
            if (g_card_entry_suffix_values[entry_index] == group &&
                strncmp(g_lom_pocketstation_filename_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) == 0)
            {
                lom_native_save_copy((u8*)&g_card_entries[g_card_slot][entry_index], (u8*)&sorted_entries[output_count], sizeof(struct DIRENTRY));
                output_count++;
            }
        }
    }

    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        if (strncmp(g_new_save_entry_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_MENU_NEW_SAVE_NAME_LENGTH) == 0 ||
            strncmp(g_card_full_entry_name, g_card_entries[g_card_slot][entry_index].name, CARDA_CARD_FULL_ENTRY_NAME_LENGTH) == 0)
        {
            lom_native_save_copy((u8*)&g_card_entries[g_card_slot][entry_index], (u8*)&sorted_entries[output_count], sizeof(struct DIRENTRY));
            output_count++;
        }
    }

    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        if (strncmp(g_lom_save_filename_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) != 0 &&
            strncmp(g_lom_pocketstation_filename_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_SAVE_FILENAME_PREFIX_LENGTH) != 0 &&
            strncmp(g_new_save_entry_prefix, g_card_entries[g_card_slot][entry_index].name, CARD_MENU_NEW_SAVE_NAME_LENGTH) != 0 &&
            strncmp(g_card_full_entry_name, g_card_entries[g_card_slot][entry_index].name, CARDA_CARD_FULL_ENTRY_NAME_LENGTH) != 0)
        {
            lom_native_save_copy((u8*)&g_card_entries[g_card_slot][entry_index], (u8*)&sorted_entries[output_count], sizeof(struct DIRENTRY));
            output_count++;
        }
    }

    for (entry_index = 0; entry_index < g_card_entry_state; entry_index++)
    {
        lom_native_save_copy((u8*)&sorted_entries[entry_index], (u8*)&g_card_entries[g_card_slot][entry_index], sizeof(struct DIRENTRY));
    }
}
