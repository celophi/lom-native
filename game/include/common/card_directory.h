#ifndef CARD_DIRECTORY_H
#define CARD_DIRECTORY_H

#include "common.h"
#include "common/card_file.h"
#include <kernel.h>

#define CARD_SLOT_COUNT 2
#define CARD_DIRECTORY_ENTRY_COUNT 20

#define CARD_DIRECTORY_ENTRY_BYTES sizeof(struct DIRENTRY)
#define CARD_DIRECTORY_BYTES (CARD_DIRECTORY_ENTRY_COUNT * CARD_DIRECTORY_ENTRY_BYTES)

#define CARD_BLOCK_BYTES 8192

#define CARD_SECTOR_BYTES 128

#define CARD_BLOCK_SECTORS (CARD_BLOCK_BYTES / CARD_SECTOR_BYTES)

#define CARD_FILE_BLOCKS(count) ((count) << 16)

#define CARD_DEVICE_PREFIX "bu00:"
#define CARD_SEARCH_PATTERN "bu00:*"

#define CARD_DEVICE_SLOT_DIGIT 2

#define CARD_DEVICE_BYTES sizeof(CARD_DEVICE_PREFIX)
#define CARD_SEARCH_PATTERN_BYTES sizeof(CARD_SEARCH_PATTERN)

typedef union
{
    u32 word;

    struct
    {
        u8 name[2];
        u8 slot;
        u8 port;
    } characters;
} CardDevice;

typedef union
{
    char text[8];
    CardDevice device;
} CardPathTemplate;

typedef union
{
    char text[16];
    CardDevice device;
} CardSearchPattern;

typedef union
{
    char text[32];
    CardDevice device;
} CardFilePath;

typedef union
{
    char text[104];
    CardDevice device;
} CardSequencePath;

typedef union
{
    char text[256];
    CardDevice device;
} CardEntryPath;

#define CARD_SAVE_FILENAME_PREFIX_LENGTH 12

#define CARD_SERIAL_DIGITS 5

#define CARD_SERIAL_DIGIT_BITS 4

#define CARD_SERIAL_DECIMAL_DIGITS 10

#define CARD_FILE_ERROR (-1)

#define CARD_ENTRY_SERIAL_NONE (-1)

#define CARD_ENTRY_SERIAL_TEXT(card, index)                                                                                                                    \
    (((card) * CARD_DIRECTORY_BYTES + (index) * CARD_DIRECTORY_ENTRY_BYTES) + (u8*)&g_card_entries[0][0].name[CARD_SAVE_FILENAME_PREFIX_LENGTH])

extern struct DIRENTRY g_card_entries[CARD_SLOT_COUNT][CARD_DIRECTORY_ENTRY_COUNT];

extern s32 g_card_entry_state;

extern s32 g_card_entry_fields[CARD_SLOT_COUNT][CARD_DIRECTORY_ENTRY_COUNT];

extern s32 g_card_entry_suffix_values[];

extern char g_lom_save_filename_prefix[];

extern char g_lom_pocketstation_filename_prefix[];

extern char g_new_save_entry_prefix[];

extern char g_lom_save_dummy_filename[];
extern char g_lom_pocketstation_dummy_filename[];

s32 parse_entry_fields(void);

/**
 * @brief Set the SavedGameLayout.known_save_flags bit of every product code that @p file_name starts with (FIELD).
 * @param file_name Memory-card file name of one directory entry.
 */
void field_flag_known_save(char* file_name);

#define CARD_SAVE_SLOTS 15
#define CARD_USED_BLOCK_LIMIT 14

extern s32 g_card_entry_ranks[];

extern s32 g_card_rank_count;

#define CARD_RANK_COUNT_NONE 40

extern s32 g_card_entry_value_limit;

extern char g_card_selected_save_path[];

extern char g_card_temp_save_path[];

s32 card_rank_entries(void);
void card_reset_entry_ranks(void);
s32 card_has_known_entry_type(void);
s32 card_entry_blocks_reach_limit(void);
void card_erase_placeholder_files(void);
void card_sort_entries_by_type(void);

#endif
