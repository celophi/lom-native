#include "common.h"
#include "main/main.h"
#include <strings.h>

#define FIELD_KNOWN_SAVE_CODE_COUNT 11

#define FIELD_KNOWN_SAVE_CODE_LENGTH 12

extern char g_field_known_save_codes[FIELD_KNOWN_SAVE_CODE_COUNT][FIELD_KNOWN_SAVE_CODE_LENGTH];

/**
 * @brief Set the SavedGameLayout.known_save_flags bit of every product code that @p file_name starts with.
 * @param file_name Memory-card file name of one directory entry.
 */
void field_flag_known_save(char* file_name)
{
    s32 i;

    for (i = 0; i < FIELD_KNOWN_SAVE_CODE_COUNT; i++)
    {
        if (strncmp(file_name, g_field_known_save_codes[i], FIELD_KNOWN_SAVE_CODE_LENGTH) == 0)
        {
            g_saved_game_ctx->words[SAVED_GAME_WORD_KNOWN_SAVES] |= 1 << i;
        }
    }
}
