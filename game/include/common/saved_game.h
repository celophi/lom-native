#ifndef SAVED_GAME_H
#define SAVED_GAME_H

#include "common.h"

#include "common/card_file.h"

#define SAVED_GAME_DATA_SIZE 0x3268
#define SAVED_GAME_BUFFER_SIZE 0x4000
#define SAVED_CHARACTER_SIZE 0x250

#define SAVED_OPTION_GAME_CLEARED 0x04
#define SAVED_OPTION_WORLD_SELECT_PENDING 0x08

#define FIELD_WEEKDAY_MASK 0x7F

#define SAVED_GAME_WORD_KNOWN_SAVES 0x48

#define SAVED_GAME_WORD_EVENT_PROGRESS 8

#define SAVED_GAME_WORD_MODE_FLAGS 0x7F

#define FIELD_CONTROL_RESET_WORDS 0x800000

#define FIELD_SPAWN_LOAD_GAME 6

#define ITEM_COUNT_MAX 99

#define SAVED_GAME_SUMMARY_NAME_BYTES 21

#define SAVED_GAME_PENDING_ITEM_COUNT 4

#define SAVE_NO_ICON 0x7F

#define SAVE_ICON_HERO_COUNT 2
#define SAVE_ICON_PET_BASE 0x0E
#define SAVE_ICON_GOLEM_BASE 0x4F

#define SAVED_PLAY_TIME_TICKS_PER_MINUTE 3600

#define SAVED_PLAY_TIME_MINUTES_PER_HOUR 60

#define SAVED_PLAY_TIME_TICKS_PER_HOUR (SAVED_PLAY_TIME_TICKS_PER_MINUTE * SAVED_PLAY_TIME_MINUTES_PER_HOUR)

#define SAVED_MONEY_MAX 10000000

#define FIELD_PARTY_SIZE 3
#define FIELD_PARTY_HERO 0
#define FIELD_PARTY_GUEST 1
#define FIELD_PARTY_COMPANION 2
#define FIELD_LAND_COUNT 64
#define FIELD_ITEM_COUNT 100
#define FIELD_ITEM_KIND_COUNT 256
#define FIELD_EQUIPMENT_SLOT_COUNT 4
#define FIELD_WEAPON_SLOT 0
#define FIELD_ARMOR_SLOT (FIELD_WEAPON_SLOT + 1)
#define FIELD_ITEM_SPECIAL_COUNT 4
#define FIELD_CHARACTER_STAT_COUNT 8
#define FIELD_COMMAND_SLOT_COUNT 2
#define FIELD_SKILL_SLOT_COUNT 4
#define FIELD_SKILL_NONE 0xFF
#define FIELD_SKILL_INSTRUMENT 0x80
#define PET_RECORD_COUNT 5
#define SAVED_BITS_PER_WORD 32
#define SAVED_BIT_IS_SET(words, bit) ((words)[(bit) / SAVED_BITS_PER_WORD] & (1 << ((bit) % SAVED_BITS_PER_WORD)))
#define SAVED_BIT_SET(words, bit) ((words)[(bit) / SAVED_BITS_PER_WORD] = (words)[(bit) / SAVED_BITS_PER_WORD] | (1 << ((bit) % SAVED_BITS_PER_WORD)))
#define SAVED_BIT_CLEAR(words, bit) ((words)[(bit) / SAVED_BITS_PER_WORD] &= ~(1 << ((bit) % SAVED_BITS_PER_WORD)))
#define FIELD_ENCYCLOPEDIA_BIT_WORDS 32
#define SAVED_GAME_FLAG_WORDS 2
#define SAVED_GAME_FLAG_COUNT (SAVED_GAME_FLAG_WORDS * SAVED_BITS_PER_WORD)
#define SAVED_EVENT_FLAG_NEW_GAME 28
#define FIELD_ENCYCLOPEDIA_LAND_COUNT FIELD_LAND_LUCEMIA_2
#define FIELD_ENCYCLOPEDIA_LAND_BIT 0x200
#define FIELD_ENCYCLOPEDIA_ARTIFACT_BIT 0x240
#define FIELD_ENCYCLOPEDIA_ARTIFACT_PLACED_BIT 0x280
#define FIELD_WEAPON_CATEGORY_COUNT 11
#define FIELD_TECHNIQUES_PER_WEAPON 24
#define FIELD_ABILITY_COUNT 88
#define FIELD_ABILITY_WORD_COUNT 3

#define FIELD_CHARACTER_TYPE_MASK 0x7F

#define FIELD_CHARACTER_GUEST 2

#define FIELD_CHARACTER_COMPANION 3

#define FIELD_CHARACTER_GOLEM 4

#define FIELD_CHARACTER_PAD_CONTROLLED 0x80
#define FIELD_CHARACTER_PAD_CONTROLLED_SHIFT 7

#define SAVE_TAG_ANY 0xFF
#define SAVE_TAG_STARTUP 7

typedef union
{
    u32 word;

    struct
    {
        u16 game_id;
        u16 save_id;
    } ids;
} SaveIdentity;

#define FIELD_ITEM_NAME_LENGTH 0x14

#define FIELD_ITEM_CATEGORY_SHIFT 8
#define FIELD_ITEM_CATEGORY_MASK 3
#define FIELD_ITEM_CATEGORY_BITS (FIELD_ITEM_CATEGORY_MASK << FIELD_ITEM_CATEGORY_SHIFT)

#define FIELD_ITEM_CATEGORY(info) (((info) >> FIELD_ITEM_CATEGORY_SHIFT) & FIELD_ITEM_CATEGORY_MASK)

#define FIELD_ITEM_MATERIAL_MASK 0x3F

#define FIELD_ITEM_TYPE_SHIFT 10
#define FIELD_ITEM_TYPE_MASK 0x3F
#define FIELD_ITEM_TYPE_BITS (FIELD_ITEM_TYPE_MASK << FIELD_ITEM_TYPE_SHIFT)

#define FIELD_ITEM_TYPE(info) (((info) >> FIELD_ITEM_TYPE_SHIFT) & FIELD_ITEM_TYPE_MASK)
#define FIELD_ITEM_TYPE_TABLE_OFFSET(info) (((info) >> (FIELD_ITEM_TYPE_SHIFT - 1)) & (FIELD_ITEM_TYPE_MASK << 1))

enum
{
    FIELD_ITEM_CATEGORY_WEAPON = 0,
    FIELD_ITEM_CATEGORY_ARMOR = 1,
    FIELD_ITEM_CATEGORY_INSTRUMENT = 2
};

enum
{
    FIELD_WEAPON_KNIFE = 0,
    FIELD_WEAPON_SWORD = 1,
    FIELD_WEAPON_AXE = 2,
    FIELD_WEAPON_TWO_HANDED_SWORD = 3,
    FIELD_WEAPON_TWO_HANDED_AXE = 4,
    FIELD_WEAPON_HAMMER = 5,
    FIELD_WEAPON_SPEAR = 6,
    FIELD_WEAPON_STAFF = 7,
    FIELD_WEAPON_GLOVE = 8,
    FIELD_WEAPON_FLAIL = 9,
    FIELD_WEAPON_BOW = 10
};

#define FIELD_WEAPON_TYPE_COUNT 11

enum
{
    FIELD_ARMOR_SHIELD = 0,
    FIELD_ARMOR_HELM = 1,
    FIELD_ARMOR_HAT = 2,
    FIELD_ARMOR_HAUBERK = 3,
    FIELD_ARMOR_ROBE = 4,
    FIELD_ARMOR_GAUNTLET = 5,
    FIELD_ARMOR_RING = 6,
    FIELD_ARMOR_BOOTS = 7,
    FIELD_ARMOR_SHOES = 8,
    FIELD_ARMOR_ARMOR = 9,
    FIELD_ARMOR_MANTLE = 10,
    FIELD_ARMOR_PENDANT = 11
};

#define FIELD_STAT_EFFECTIVE_SHIFT 9

#define FIELD_STAT_BASE_MASK 0x1FF

#define FIELD_STAT_EFFECTIVE_MASK 0xFE00

typedef union FieldStat
{
    u16 value;

    struct
    {
        u16 base : 9;
        u16 effective : 7;
    } bits;
} FieldStat;

typedef struct FieldItemModifiers
{
    u32 stat0 : 4;
    u32 stat1 : 4;
    u32 stat2 : 4;
    u32 stat3 : 4;
    u32 stat4 : 4;
    u32 stat5 : 4;
    u32 stat6 : 4;
    u32 stat7 : 4;
} FieldItemModifiers;

typedef struct FieldNibbles
{
    unsigned n0 : 4;
    unsigned n1 : 4;
    unsigned n2 : 4;
    unsigned n3 : 4;
    unsigned n4 : 4;
    unsigned n5 : 4;
    unsigned n6 : 4;
    unsigned n7 : 4;
} FieldNibbles;

typedef struct FieldItemInfo
{
    unsigned unused_0 : 8;
    unsigned category : 2;
    unsigned item_type : 6;
    unsigned material : 6;
    unsigned unused_22 : 10;
} FieldItemInfo;

typedef struct FieldItemKey
{
    s32 first;
    s32 second;
} FieldItemKey;

typedef struct FieldItemRecord
{
    u8 name[FIELD_ITEM_NAME_LENGTH];

    union
    {
        u32 word;
        FieldItemInfo bits;
        u16 halves[2];
    } info;

    union
    {
        u32 word;
        FieldNibbles bits;
        u8 bytes[4];
        u16 halves[2];
    } bonus_nibbles;

    union
    {
        u32 word;
        FieldNibbles bits;
        u8 bytes[4];
        u16 halves[2];
    } stat_nibbles;

    u8 special_ids[FIELD_ITEM_SPECIAL_COUNT];

    union
    {
        u8 bytes[8];
        u16 values[4];
        s16 signed_values[4];

        struct
        {
            u16 power;
            u8 stats[6];
        } weapon;

        struct
        {
            u8 spirit;
            u8 spell;
            u8 power;
        } instrument;

        struct
        {
            u32 logic_class : 4;
            u32 grid_bound : 4;
            u32 unused_8 : 8;
            u32 failure_rate : 8;
            u32 unused_24 : 8;
            s32 palette;
        } golem;
    } derived;

    u8 status_flags;
    u8 element_flags;
    u16 effect_index;
    u8 attributes[4];
    s32 value;
    FieldItemKey key;
} FieldItemRecord;

typedef struct FieldCharacterRecord
{
    u8 name[24];

    union
    {
        u32 word;
        u8 bytes[8];

        struct
        {
            u8 type : 7;
            u8 pad_controlled : 1;
        } bits;

        struct
        {
            u8 type;
            u8 variant;
            u8 commands[FIELD_COMMAND_SLOT_COUNT];
            u8 skills[FIELD_SKILL_SLOT_COUNT];
        } actions;
    } info;

    union
    {
        u32 word;
        u8 level;

        struct
        {
            unsigned level : 8;
            unsigned experience : 24;
        } bits;
    } progress;

    u16 hp;
    u16 power;
    u16 equipment_totals[4];
    u16 stats[FIELD_CHARACTER_STAT_COUNT];
    u8 status_immunities;
    u8 weapon_flags;
    u8 element_resistances;
    u8 companion_ability;
    u16 duel_wins;
    u16 duel_losses;
    u8 button_actions[8];
    FieldItemRecord equipment[FIELD_EQUIPMENT_SLOT_COUNT];
    FieldItemRecord instruments[4];
} FieldCharacterRecord;

#define FIELD_LAND_ARTIFACT_OBTAINED 0x01
#define FIELD_LAND_PLACED 0x02
#define FIELD_LAND_CLEARED 0x04
#define FIELD_LAND_ARTIFACT_UNLOCKED 0x08
#define FIELD_LAND_GET_PLACED(flags) (((flags) >> 1) & 1)
#define FIELD_LAND_ENTRY_SLOT(flags) (((flags) >> 4) & 3)
#define FIELD_LAND_CELL_NONE 15

#define FIELD_LAND_SPIRIT_COUNT 8
#define FIELD_LAND_SPIRIT_NEUTRAL 3
#define FIELD_LAND_SPIRIT_MAX 6

typedef enum FieldSpirit
{
    FIELD_SPIRIT_WISP = 0,
    FIELD_SPIRIT_SHADE = 1,
    FIELD_SPIRIT_DRYAD = 2,
    FIELD_SPIRIT_AURA = 3,
    FIELD_SPIRIT_SALAMANDER = 4,
    FIELD_SPIRIT_GNOME = 5,
    FIELD_SPIRIT_JINN = 6,
    FIELD_SPIRIT_UNDINE = 7
} FieldSpirit;

typedef enum FieldLandId
{
    FIELD_LAND_HOME = 0,
    FIELD_LAND_DOMINA = 1,
    FIELD_LAND_GATO_GROTTOES = 2,
    FIELD_LAND_POLPOTA_HARBOR = 3,
    FIELD_LAND_LUMINA = 4,
    FIELD_LAND_GEO = 5,
    FIELD_LAND_UNNAMED_06 = 6,
    FIELD_LAND_MADORA_BEACH = 7,
    FIELD_LAND_ULKAN_MINES = 8,
    FIELD_LAND_BONE_FORTRESS = 9,
    FIELD_LAND_LAKE_KILMA = 10,
    FIELD_LAND_TOWER_OF_LEIRES = 11,
    FIELD_LAND_WHITE_FOREST = 12,
    FIELD_LAND_MEKIV_CAVERNS = 13,
    FIELD_LAND_UNUSED_14 = 14,
    FIELD_LAND_NORN_PEAKS = 15,
    FIELD_LAND_UNDERWORLD = 16,
    FIELD_LAND_DUMA_DESERT = 17,
    FIELD_LAND_LUON_HIGHWAY = 18,
    FIELD_LAND_JUNKYARD = 19,
    FIELD_LAND_UNUSED_20 = 20,
    FIELD_LAND_JUNGLE = 21,
    FIELD_LAND_UNUSED_22 = 22,
    FIELD_LAND_FLAMES = 23,
    FIELD_LAND_LUCEMIA = 24,
    FIELD_LAND_BEJEWELED_CITY = 25,
    FIELD_LAND_FIEG_SNOWFIELDS = 26,
    FIELD_LAND_SS_BUCCANEER = 27,
    FIELD_LAND_UNUSED_28 = 28,
    FIELD_LAND_UNUSED_29 = 29,
    FIELD_LAND_MINDAS_RUINS = 30,
    FIELD_LAND_TREE_OF_MANA = 31,
    FIELD_LAND_ORCHARDS = 32,
    FIELD_LAND_LUCEMIA_2 = 33
} FieldLandId;

typedef struct FieldLandRecord
{
    u8 flags;
    u8 x : 4;
    u8 z : 4;
    u8 placement_order;
    u8 artifact_order;
    u8 levels[FIELD_LAND_SPIRIT_COUNT];
} FieldLandRecord;

typedef union FieldLandWords
{
    FieldLandRecord record;
    u32 word;

    struct
    {
        u32 artifact_obtained : 1;
        u32 placed : 1;
        u32 cleared : 1;
        u32 artifact_unlocked : 1;
        u32 entry_slot : 2;
        u32 unused_6 : 2;
        u32 x : 4;
        u32 z : 4;
        u32 placement_order : 8;
        u32 artifact_order : 8;
    } bits;

    struct
    {
        u8 flags;
        u8 cell;
        u8 placement_order;
        u8 artifact_order;
    } bytes;
} FieldLandWords;

#define FIELD_LAND_WORD_X(word) ((u8)(((word) >> 8) & 0xF))
#define FIELD_LAND_WORD_Z(word) ((u8)(((word) >> 12) & 0xF))

#define FIELD_MENU_ENTRY_EMPTY 0xFF

#define FIELD_EFFECT_ID_BASE 0x60

#define FIELD_EFFECT_COUNT 40

#define FIELD_MENU_SLOT_UNUSED 0
#define FIELD_MENU_SLOT_PLAIN 1
#define FIELD_MENU_SLOT_ITEM 2
#define FIELD_MENU_SLOT_INDEXED 3

#define FIELD_MENU_SLOT_COUNTER_COUNT 8

typedef struct FieldMenuSlot
{
    s32 handle;

    union
    {
        u32 word;
        u8 index;

        struct
        {
            u32 index : 8;
            u32 result_type : 2;
            u32 unused_10 : 22;
        } bits;
    } entry;

    u8 counters[FIELD_MENU_SLOT_COUNTER_COUNT];
} FieldMenuSlot;

#define FIELD_MENU_GROUP_ITEM_COUNT 4

#define FIELD_MENU_GROUP_SLOT_COUNT 8

#define FIELD_MENU_GROUP_COUNT 4

typedef struct FieldMenuSlotGroup
{
    union
    {
        u32 word;

        struct
        {
            u32 capacity : 4;
            u32 unused_4 : 4;
            u32 item_slot_count : 4;
            u32 item_count : 4;
            u32 unused_16 : 16;
        } bits;
    } flags;

    u8 pad4[4];
    u8 items[FIELD_MENU_GROUP_ITEM_COUNT];
    FieldMenuSlot slots[FIELD_MENU_GROUP_SLOT_COUNT];
} FieldMenuSlotGroup;

typedef union FieldGrowth
{
    u8 byte;

    struct
    {
        u8 rate : 4;
        u8 accumulator : 4;
    } bits;
} FieldGrowth;

#define FIELD_COMPANION_NAME_LENGTH 21

typedef struct
{
    u32 pending_effects : 24;
    u32 applied_effects : 3;
    u32 unused_27 : 3;
    u32 grazing : 1;
    u32 egg : 1;
} PetStatusFlags;

#define PET_STATUS_EGG 0x80000000

#define PET_NAME_LENGTH 21

typedef struct PetRecord
{
    u8 name[PET_NAME_LENGTH];
    u8 species;
    u8 egg_species;
    u8 unused_17;

    union
    {
        u32 word;
        u8 level;

        struct
        {
            unsigned level : 8;
            unsigned experience : 24;
        } bits;
    } progress;

    u16 hp;
    u16 power;
    u16 equipment_totals[4];
    u16 stats[FIELD_CHARACTER_STAT_COUNT];
    u8 status_immunities;
    u8 weapon_flags;
    u8 element_resistances;
    u8 companion_ability;
    u8 weapon_id;
    u8 armor_ids[3];
    u8 unused_34[2];
    u16 hatch_counter;

    union
    {
        u32 word;
        u8 effects[4];
        PetStatusFlags bits;
    } status;

    union
    {
        s32 word;
        u8 low;
    } companion_flags;

    FieldGrowth stat_growth[FIELD_CHARACTER_STAT_COUNT];
    FieldGrowth total_growth[4];

    union
    {
        u32 word;
        u8 bytes[4];
        u16 halves[2];

        struct
        {
            u8 rate : 4;
            u32 accumulator : 4;
            u32 price_growth : 8;
            u32 sale_price : 16;
        } growth;
    } extra_growth;

    s32 unique_id;
} PetRecord;

#define GOLEM_GRID_CELL_COUNT 36
#define GOLEM_GRID_WIDTH 6
#define GOLEM_GRID_EMPTY 99

typedef struct
{
    u8 block_id;
    u8 detail;
    u8 edge;
    u8 owner;
} GolemGridCell;

#define GOLEM_RECORD_COUNT 3
#define COMPANION_STAT_COUNT 4
#define GOLEM_COUNT_MASK 0xF
#define GOLEM_SAVED_GROUP_MASK 0xF0
#define GOLEM_SAVED_GROUP_SHIFT 4
#define GOLEM_NO_GROUP 3
#define SAVED_GAME_GOLEM_HEADER_WORD(layout) (*(u32*)&(layout).golem_count)
#define LOGIC_BLOCK_CAPACITY 40
#define LOGIC_BLOCK_UNASSIGNED 3

typedef union
{
    u32 word;

    struct
    {
        u32 logic_type : 2;
        u32 id : 6;
        u32 level : 4;
        u32 shape : 4;
        u32 placed : 1;
        u32 rotation : 2;
        s32 grid_x : 5;
        s32 grid_y : 5;
        u32 unused_29 : 3;
    } f;
} LogicBlock;

#define GOLEM_LOGIC_CLASS_MASK 0xF
#define GOLEM_GRID_BOUND_SHIFT 4

#define GOLEM_SOURCE_ITEM_COUNT 4

#define GOLEM_NAME_LENGTH 21

typedef struct
{
    u8 name[GOLEM_NAME_LENGTH];
    u8 unused_15;
    u16 hp;
    u16 power;
    u16 equipment_totals[COMPANION_STAT_COUNT];
    u8 unused_22[2];
    FieldNibbles weapon_bonus;
    FieldNibbles armor_bonus;
    FieldStat stats[FIELD_CHARACTER_STAT_COUNT];
    u8 armor_flags;
    u8 weapon_flags;
    u8 armor_flags2;
    u8 companion_ability;
    u32 unused_40;

    union
    {
        u8 byte;

        struct
        {
            u8 logic_class : 4;
            u8 grid_bound : 4;
        } bits;
    } logic_layout;

    u8 unused_45;
    u8 failure_rate;
    u8 unused_47;
    s32 palette;
    FieldItemRecord source_items[GOLEM_SOURCE_ITEM_COUNT];
} GolemRecord;

typedef struct SavedGameLayout
{
    u8 summary_name[SAVED_GAME_SUMMARY_NAME_BYTES];
    u8 summary_level;
    u8 summary_weapon_power;
    u8 summary_slot_count;

    union
    {
        s32 word;

        struct
        {
            u32 id : 25;
            u32 party_icon_0 : 7;
        } bits;
    } spawn;

    s16 sound_bank_id;
    s8 secondary_music_id;
    u8 icon_palette;

    union
    {
        u32 word;

        struct
        {
            u32 current_land : 18;
            u32 party_icon_1 : 7;
            u32 party_icon_2 : 7;
        } bits;
    } location;

    u16 scene_id;
    u8 object_id;
    u8 music_id;

    union
    {
        u32 word;

        struct
        {
            u32 vibration : 1;
            u32 stereo : 1;
            u32 game_cleared : 1;
            u32 world_select_pending : 1;
            u32 unused_4 : 28;
        } bits;
    } options;

    u32 money;
    s32 play_time;
    u32 technique_bits[FIELD_WEAPON_CATEGORY_COUNT];
    u32 ability_bits[FIELD_ABILITY_WORD_COUNT];
    u8 ability_proficiency[FIELD_ABILITY_COUNT];
    u8 weapon_proficiency[FIELD_WEAPON_CATEGORY_COUNT];
    u8 compatibility_tag;
    u8 padD0[0xD4 - 0xD0];
    SaveIdentity identity;
    SaveIdentity guest_origin;
    u8 padDC[2];
    u16 guest_loaded;
    s32 world_map_cell;
    s32 words[0x80];

    union
    {
        u32 word;

        struct
        {
            u8 artifact_count;
            u8 placed_land_count;
            u16 weekday;
        } fields;

        struct
        {
            u32 artifact_count : 8;
            u32 placed_land_count : 8;
            u32 weekday : 7;
            u32 reset_words : 1;
            u32 unused_24 : 8;
        } bits;
    } control;

    u32 flag_bits[SAVED_GAME_FLAG_WORDS];
    FieldLandRecord lands[FIELD_LAND_COUNT];
    FieldCharacterRecord characters[FIELD_PARTY_SIZE];
    FieldItemRecord items[FIELD_ITEM_COUNT];
    u8 item_counts[FIELD_ITEM_KIND_COUNT];
    u8 pad26E0[0x26E4 - 0x26E0];
    FieldMenuSlotGroup menu_slots[FIELD_MENU_GROUP_COUNT];
    u8 pad2914[0x29D4 - 0x2914];
    u8 golem_count;
    u8 golems_created;
    u8 logic_block_count;
    s8 joined_golem;
    u8 golem_order[GOLEM_RECORD_COUNT];
    u8 golem_display_order;
    LogicBlock logic_blocks[LOGIC_BLOCK_CAPACITY];
    GolemGridCell golem_grid[GOLEM_GRID_CELL_COUNT];
    GolemRecord golem_records[GOLEM_RECORD_COUNT];
    s32 joined_pet;
    PetRecord pets[PET_RECORD_COUNT];
    u32 encyclopedia_bits[FIELD_ENCYCLOPEDIA_BIT_WORDS];
    s32 hero_hits_taken;
    s32 enemies_defeated;
    s32 retry_count;
    FieldItemRecord pending_items[SAVED_GAME_PENDING_ITEM_COUNT];
} SavedGameLayout;

#define SAVE_FILE_BYTES 0x4000

#define SAVE_FILE_CHECKSUM_BYTES 0x33E0

#define SAVE_FILE_MAGIC 0x00414E41

#define SAVE_FILE_CHECKSUM_MULTIPLIER 2

#define SAVE_FILE_CHECKSUM_BIAS 0x0414E410

#define SAVE_FILE_ICON_FRAME_COUNT 2

typedef struct
{
    char magic[2];
    u8 icon_type;
    u8 block_count;
    u8 title[2][CARD_FILE_TITLE_LINE_BYTES];
    u8 reserved[CARD_FILE_RESERVED_BYTES];
    u8 clut[CARD_FILE_ICON_CLUT_BYTES];
    u8 icon_frames[SAVE_FILE_ICON_FRAME_COUNT][CARD_FILE_ICON_FRAME_BYTES];
} SaveFileHeader;

typedef struct
{
    SaveFileHeader header;
    SavedGameLayout saved_game;
    s32 checksum;
    s32 magic;
    u8 unused[SAVE_FILE_BYTES - sizeof(SaveFileHeader) - sizeof(SavedGameLayout) - 2 * sizeof(s32)];
} SaveFile;

typedef union
{
    SavedGameLayout layout;
    u8 bytes[SAVED_GAME_BUFFER_SIZE];
    s32 words[SAVED_GAME_BUFFER_SIZE / sizeof(s32)];

    struct
    {
        u8 _pad[0x2F0];
        FieldLandWords lands[FIELD_LAND_COUNT];
    } land_words;

    struct
    {
        u8 _pad[0x640];
        FieldItemRecord records[FIELD_EQUIPMENT_SLOT_COUNT * 2];
    } hero_items;
} SavedGame;

extern SavedGame g_saved_game;

#define SAVED_GAME_FLAG_WORD_INDEX (OFFSETOF(SavedGameLayout, flag_bits) / sizeof(s32))

extern u32 g_saved_encyclopedia_bits[FIELD_ENCYCLOPEDIA_BIT_WORDS];

extern u32 g_saved_flag_bits[SAVED_GAME_FLAG_WORDS];

extern FieldLandWords g_saved_lands[FIELD_LAND_COUNT];

extern SavedGameLayout* g_saved_game_ctx;

#endif
