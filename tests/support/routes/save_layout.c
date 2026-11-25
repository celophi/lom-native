#include "common/saved_game.h"
#include "common/card_file.h"
#include <stddef.h>
#include <stdio.h>

typedef struct
{
    const char* name;

    size_t offset;

    size_t size;
} LayoutField;

#define FIELD(member) {#member, offsetof(SavedGameLayout, member), sizeof(((SavedGameLayout*)0)->member)}
#define FIELD_AS(name, member) {name, offsetof(SavedGameLayout, member), sizeof(((SavedGameLayout*)0)->member)}

/* clang-format off */
static const LayoutField s_fields[] = {
    FIELD(summary_name),
    FIELD(summary_level),
    FIELD(spawn),
    FIELD(sound_bank_id),
    FIELD(secondary_music_id),
    FIELD_AS("track", location),
    FIELD(scene_id),
    FIELD(object_id),
    FIELD(music_id),
    FIELD(options),
    FIELD(money),
    FIELD(play_time),
    FIELD(compatibility_tag),
    FIELD(words),
    FIELD(characters),
    FIELD(flag_bits),
    FIELD(lands),
    FIELD(item_counts),
    FIELD(encyclopedia_bits),
    FIELD(logic_block_count),
    FIELD(logic_blocks),
    FIELD(pets),
};
/* clang-format on */

/**
 * @brief Print the layout.
 * @return 0.
 */
int main(void)
{
    size_t count = sizeof(s_fields) / sizeof(s_fields[0]);

    printf("{\n");
    printf("  \"save_file_bytes\": %d,\n", SAVE_FILE_BYTES);
    printf("  \"saved_game_offset\": %zu,\n", offsetof(SaveFile, saved_game));
    printf("  \"saved_game_bytes\": %d,\n", SAVED_GAME_DATA_SIZE);
    printf("  \"checksum_offset\": %zu,\n", offsetof(SaveFile, checksum));
    printf("  \"magic_offset\": %zu,\n", offsetof(SaveFile, magic));
    printf("  \"checksum_bytes\": %d,\n", SAVE_FILE_CHECKSUM_BYTES);
    printf("  \"checksum_multiplier\": %d,\n", SAVE_FILE_CHECKSUM_MULTIPLIER);
    printf("  \"checksum_bias\": %d,\n", SAVE_FILE_CHECKSUM_BIAS);
    printf("  \"magic\": %d,\n", SAVE_FILE_MAGIC);
    printf("  \"tag_any\": %d,\n", SAVE_TAG_ANY);
    printf("  \"card_title_line_bytes\": %d,\n", CARD_FILE_TITLE_LINE_BYTES);
    printf("  \"character_bytes\": %zu,\n", sizeof(FieldCharacterRecord));
    printf("  \"pet_bytes\": %zu,\n", sizeof(PetRecord));
    printf("  \"pet_hatch_counter_offset\": %zu,\n", offsetof(PetRecord, hatch_counter));
    printf("  \"pet_status_offset\": %zu,\n", offsetof(PetRecord, status));
    printf("  \"fields\": {\n");

    for (size_t i = 0; i < count; i++)
    {
        printf("    \"%s\": [%zu, %zu]%s\n", s_fields[i].name, s_fields[i].offset, s_fields[i].size, i + 1 < count ? "," : "");
    }

    printf("  }\n");
    printf("}\n");
    return 0;
}
