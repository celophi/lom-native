#ifndef LOM_MENU_INTERNAL_H
#define LOM_MENU_INTERNAL_H

#include "common.h"
#include "lom_native_gpu_packets.h"

#include "common/saved_game.h"
#include "common/vector.h"
#include "main/audio/akao.h"
#include "main/audio/game_audio.h"
#include "main/controller.h"
#include "main/display.h"
#include "overlays/field/field_numeric_sprites.h"
#include "overlays/field/field_object_state.h"
#include "overlays/field/field_overlay_handoff.h"
#include "overlays/field/field_player_records.h"
#include "overlays/field/field_portrait.h"
#include "overlays/field/field_sound.h"
#include "overlays/field/field_text.h"
#include "overlays/field/field_ui_text.h"
#include "overlays/menu/menu.h"

void field_copy_name(u8* destination, u8* source);
s32 field_name_byte_length(u8* name);

s32 field_get_next_level_experience(s32 index);

#define MENU_CLUT_GRID_BASE 0x7C80
#define MENU_CLUT_GRID_ALT 0x7C81
#define MENU_CLUT_CORNER 0x7CCA
#define MENU_GRID_OT_INDEX 0x0F
#define MENU_GRID_ALT_CLUT_START 0x11
#define MENU_GRID_SPRITE_COUNT 0x1D
#define MENU_GRID_TEXTURE_WINDOW_SIZE 0xFF
#define MENU_GRID_TPAGE 5
#define MENU_SCROLL_ARROW_SIZE 16
#define MENU_SCROLL_ARROW_UV_UP 0x1080
#define MENU_SCROLL_ARROW_UV_DOWN 0x2080
#define MENU_SCROLL_ARROW_CLUT 0x7C86

#define MENU_SLOT_COUNT 4
#define MENU_SLOT_STATE_FREE 0
#define MENU_SLOT_STATE_OPENING 1
#define MENU_SLOT_STATE_OPEN 2
#define MENU_SLOT_STATE_CLOSING 3
#define MENU_SLOT_OT_INDEX_SHIFT 25
#define MENU_SLOT_OT_INDEX_CLEAR_MASK 0x01FFFFFF
#define MENU_WINDOW_TRANSITION_STEPS 12
#define MENU_WINDOW_OPEN_FRAME (MENU_WINDOW_TRANSITION_STEPS / 2 - 1)
#define MENU_WINDOW_MIN_WIDTH 0x20
#define MENU_WINDOW_MIN_HEIGHT 0x10

#define MENU_TIM_IMAGE_BLOCK_SIZE 0x800C

#define MENU_TW_CORNER_TL 0x70D0
#define MENU_TW_CORNER_TR 0x70D8
#define MENU_TW_CORNER_BL 0x78D0
#define MENU_TW_CORNER_BR 0x78D8
#define MENU_TW_EDGE_TOP 0x80D0
#define MENU_TW_EDGE_BOT 0x88D0
#define MENU_TW_EDGE_LEFT 0x90D0
#define MENU_TW_EDGE_RIGHT 0x90D8
#define MENU_TW_FILL 0xA0A0
#define MENU_WINDOW_CORNER_SIZE 8
#define MENU_WINDOW_FILL_TILE_SIZE 0x60
#define MENU_WINDOW_EDGE_TEXTURE_LONG_SIDE 16
#define MENU_WINDOW_EDGE_TEXTURE_SHORT_SIDE 8
#define MENU_LABEL_BUFFER_SIZE 16
#define MENU_TEXT_PREFIX_FIRST 0x19U
#define MENU_TEXT_PREFIX_COUNT 7U

#define MENU_NODE_COUNT 0x2C
#define MENU_NODE_FLAG_ACTIVE 0x01
#define MENU_NODE_FLAG_EXPANDED 0x02
#define MENU_NAV_X_MASK 0x7F00
#define MENU_NAV_X_CLEAR 0x80FF
#define MENU_NAV_Y0_BIT 0x8000
#define MENU_LAYOUT_Y0_BIT 0x8000
#define MENU_MAX_CHILDREN 4
#define MENU_NODE_LAYOUT_IDLE 0
#define MENU_NODE_LAYOUT_STEPS 4

typedef enum
{
    MENU_NODE_HERO_STATUS = 0x00,
    MENU_NODE_HERO_EQUIPMENT = 0x01,
    MENU_NODE_HERO_ABILITIES = 0x02,
    MENU_NODE_GUEST_HERO_STATUS = 0x03,
    MENU_NODE_GUEST_HERO_EQUIPMENT = 0x04,
    MENU_NODE_GUEST_HERO_ABILITIES = 0x05,
    MENU_NODE_GUEST_PARTNER_STATUS = 0x06,
    MENU_NODE_GUEST_PARTNER_ATTRIBUTES = 0x07,
    MENU_NODE_GUEST_PARTNER_ABILITIES = 0x08,
    MENU_NODE_CHARACTER_END = 0x09,
    MENU_NODE_PET_STATUS = 0x09,
    MENU_NODE_PET_ATTRIBUTES = 0x0A,
    MENU_NODE_PET_STATUS_UNUSED = 0x0C,
    MENU_NODE_PET_ATTRIBUTES_UNUSED = 0x0D,
    MENU_NODE_GOLEM_STATUS = 0x0F,
    MENU_NODE_GOLEM_ATTRIBUTES = 0x10,
    MENU_NODE_GOLEM_LOGIC_BLOCKS = 0x11,
    MENU_NODE_ITEMS = 0x12,
    MENU_NODE_WEAPONS = 0x13,
    MENU_NODE_WEAPON_STATUS = 0x14,
    MENU_NODE_WEAPON_ITEM_STATUS = 0x15,
    MENU_NODE_ARMOR = 0x16,
    MENU_NODE_ARMOR_STATUS = 0x17,
    MENU_NODE_ARMOR_ITEM_STATUS = 0x18,
    MENU_NODE_INSTRUMENTS = 0x19,
    MENU_NODE_INSTRUMENT_STATUS = 0x1A,
    MENU_NODE_INSTRUMENT_ITEM_STATUS = 0x1B,
    MENU_NODE_ARTIFACTS = 0x1C,
    MENU_NODE_BACKFIELD = 0x1D,
    MENU_NODE_GAME_ENVIRONMENT = 0x1E,
    MENU_NODE_BUTTONS_CHARACTER_0 = 0x1F,
    MENU_NODE_BROWSE_ALL = 0x20,
    MENU_NODE_BUTTONS_CHARACTER_1 = 0x2B
} MenuNodeId;

#define MENU_NONE 0xFF
#define MENU_ROW_HEIGHT 0x13
#define MENU_VIEW_HEIGHT 0xAB
#define MENU_LIST_ROW_HEIGHT 0x10
#define MENU_LIST_ROW_SHIFT 4
#define MENU_ITEM_NAV_POSITION_MASK 0x3FFF
#define MENU_ITEM_NAV_INDEX_MASK 0x1FF
#define MENU_ITEM_NAV_PREVIOUS_SHIFT 14
#define MENU_ITEM_NAV_NEXT_SHIFT 23
#define MENU_LIST_COUNT_SHIFT 16
#define MENU_LIST_COUNT_MASK (MENU_ITEM_NAV_INDEX_MASK << MENU_LIST_COUNT_SHIFT)
#define MENU_LIST_WITH_COUNT(packed, count) (((packed) & ~MENU_LIST_COUNT_MASK) | (((count) & MENU_ITEM_NAV_INDEX_MASK) << MENU_LIST_COUNT_SHIFT))
#define MENU_BUTTON_ITEM_FIRST 0x11
#define MENU_BUTTON_COUNT 8
#define MENU_EQUIPMENT_SUBTYPE_BASE 7
#define MENU_EQUIPMENT_SUBTYPE(slot) (MENU_EQUIPMENT_SUBTYPE_BASE + (slot))
#define MENU_COMMAND_SUBTYPE_BASE 1
#define MENU_SKILL_SUBTYPE_BASE 3
#define MENU_WEAPON_ATTACK_SUBTYPE_BASE 11
#define MENU_MONSTER_FAMILY_SUBTYPE 14
#define MENU_RENAME_ITEM_SUBTYPE 15
#define MENU_ITEM_SPECIAL_SUBTYPE_BASE 16
#define MENU_ITEM_ATTACK_SUBTYPE_BASE 19
#define MENU_INSTRUMENT_MAGIC_SUBTYPE 22
#define MENU_INSTRUMENT_SPIRIT_SUBTYPE 23
#define MENU_INSTRUMENT_SHAPE_SUBTYPE 24
#define MENU_COMPANION_ABILITY_SUBTYPE 25
#define MENU_ITEM_CATEGORY_NONE 3
#define MENU_NODES_PER_CHARACTER 3
#define MENU_CHARACTER_NODE_STATUS 0
#define MENU_CHARACTER_NODE_EQUIPMENT 1
#define MENU_CHARACTER_NODE_ABILITIES 2
#define MENU_CONTENT_OT_INDEX 3
#define MENU_ITEM_TABLE_COUNT 100
#define MENU_ITEM_TABLE_OFFSET OFFSETOF(SavedGameLayout, items)
#define MENU_ITEM_RECORD_SIZE sizeof(FieldItemRecord)
#define MENU_CHARACTER_BLOCK_SIZE sizeof(FieldCharacterRecord)
#define MENU_CHARACTER_RECORD_OFFSET OFFSETOF(SavedGameLayout, characters)
#define MENU_CHARACTER_PACKED_VALUES_OFFSET OFFSETOF(SavedGameLayout, characters[0].stats)
#define MENU_CHARACTER_ITEMS_OFFSET OFFSETOF(FieldCharacterRecord, equipment)
#define MENU_CHARACTER_FIELD_OFFSET(field) (MENU_CHARACTER_RECORD_OFFSET + OFFSETOF(FieldCharacterRecord, field))
#define MENU_CHARACTER_INFO_OFFSET MENU_CHARACTER_FIELD_OFFSET(info)
#define MENU_FIRST_EQUIPMENT_OFFSET (MENU_CHARACTER_RECORD_OFFSET + MENU_CHARACTER_ITEMS_OFFSET)
#define MENU_ITEM_NAV_PREVIOUS_CLEAR_MASK 0xFF803FFF
#define MENU_ITEM_NAV_NEXT_CLEAR_MASK 0x007FFFFF
#define MENU_ABILITY_BYTE_COUNT (FIELD_ABILITY_WORD_COUNT * 4)
#define MENU_ABILITIES_PER_BYTE 8
#define MENU_SKILL_OFFSET (MENU_CHARACTER_RECORD_OFFSET + OFFSETOF(FieldCharacterRecord, info.actions.skills))
#define MENU_ACTION_SLOT_OFFSET (MENU_CHARACTER_INFO_OFFSET + 1)
#define MENU_EVENT_WORD_COUNT 16
#define MENU_EVENTS_PER_WORD 8
#define MENU_EVENT_STAGE_BITS 4
#define MENU_EVENT_STAGE_MASK 0xF
#define MENU_EVENT_STAGE_LISTED 2
#define MENU_EVENT_STAGE_MARKED 8
#define MENU_EVENT_STAGE_COMPLETE 0xF
#define MENU_CURSOR_Y_MIN 0x0C
#define MENU_CURSOR_Y_MAX 0xA3
#define MENU_CONTENT_CURSOR_X_OFFSET 8
#define MENU_CURSOR_REVEAL_DELAY 5
#define MENU_CONTENT_X_MASK 0x1FF
#define MENU_CONTENT_X(item) ((item)->packed_x & MENU_CONTENT_X_MASK)
#define MENU_CONTENT_Y(item) ((item)->y - MENU_CONTENT_VIEW_Y_OFFSET)
#define MENU_CONTENT_STYLE_SHIFT 9
#define MENU_CONTENT_STYLE_MASK 0x7
#define MENU_CONTENT_ALIGN(item) (((item)->packed_x >> MENU_CONTENT_STYLE_SHIFT) & MENU_CONTENT_STYLE_MASK)
#define MENU_CONTENT_TYPE_SHIFT 12
#define MENU_CONTENT_VIEW_Y_OFFSET 8
#define MENU_CONTENT_ITEM_TYPE_MASK 0xF000
#define MENU_CONTENT_ITEM_TYPE_SUBMENU 0x5000
#define MENU_CONTENT_ITEM_TYPE_ACTION 0xF000
#define MENU_CONTENT_ITEM_ACTIVE_MASK 0x0E00
#define MENU_REDRAW_NAVIGATE 6
#define MENU_REDRAW_LAYOUT 8
#define MENU_PAD_INJECT_ENABLED 0x80
#define MENU_OPTION_VIBRATION_ENABLED 0x01
#define MENU_OPTION_STEREO 0x02
#define MENU_CONTENT_DIRECTION_COUNT 4
#define MENU_FRAME_OT_INDEX 13
#define MENU_TREE_DRAW_Y_OFFSET 12
#define MENU_TREE_DRAW_X 15
#define MENU_TREE_DRAW_WIDTH 36
#define MENU_TREE_DRAW_HEIGHT 170

#define MENU_CONTROLLER_ACTUATORS ((MenuControllerActuatorState*)CONTROLLER_STATE_ADDRESS)

typedef struct
{
    u32 entry_count;
    u32 tim_offset;
    u32 second_clut_offset;
    Tim tim;
    u8 image_block[MENU_TIM_IMAGE_BLOCK_SIZE];
    u16 second_clut[CLUT_ENTRY_COUNT];
} MenuTimAsset;

typedef struct
{
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} MenuRect;

typedef struct
{
    u16 x;
    u16 y;
    s16 w;
    s16 h;
} MenuRectU16;

typedef enum
{
    MENU_TEXT_GENERAL = 0,
    MENU_TEXT_MESSAGES = 1,
    MENU_TEXT_MATERIAL_NAMES = 2,
    MENU_TEXT_ABILITY_NAMES = 3,
    MENU_TEXT_ARTIFACT_HELP = 4,
    MENU_TEXT_ARTIFACT_NAMES = 5,
    MENU_TEXT_TECHNIQUE_HELP = 6,
    MENU_TEXT_TECHNIQUE_NAMES = 7,
    MENU_TEXT_SPIRIT_HELP = 8,
    MENU_TEXT_SPIRIT_NAMES = 9,
    MENU_TEXT_ITEM_HELP = 10,
    MENU_TEXT_ITEM_NAMES = 11,
    MENU_TEXT_MAGIC_SHAPE_HELP = 12,
    MENU_TEXT_MAGIC_SHAPE_NAMES = 13,
    MENU_TEXT_ABILITY_HELP = 14,
    MENU_TEXT_MAGIC_HELP = 15,
    MENU_TEXT_MAGIC_NAMES = 16,
    MENU_TEXT_MONSTER_FAMILIES = 17,
    MENU_TEXT_MONSTER_NAMES = 18,
    MENU_TEXT_MONSTER_TECHNIQUE_HELP = 19,
    MENU_TEXT_MONSTER_TECHNIQUE_NAMES = 20,
    MENU_TEXT_ITEM_SPECIAL_HELP = 21,
    MENU_TEXT_ITEM_SPECIAL_NAMES = 22,
    MENU_TEXT_WEAPON_ATTACK_HELP = 23,
    MENU_TEXT_WEAPON_ATTACK_NAMES = 24,
    MENU_TEXT_EQUIPMENT_TYPE_NAMES = 25,
    MENU_TEXT_EVENT_NAMES = 26,
    MENU_TEXT_GOLEM_PALETTE_NAMES = 27,
    MENU_TEXT_COMPANION_ABILITY_NAMES = 28,
    MENU_TEXT_COMPANION_ABILITY_HELP = 29,
    MENU_TEXT_GOLEM_GRID_SIZES = 30,
    MENU_TEXT_GOLEM_CLASS_NAMES = 31,
    MENU_TEXT_EVENT_HELP = 32,
    MENU_TEXT_ITEM_NAME_PARTS = 33,
    MENU_TEXT_TABLE_COUNT = 34
} MenuTextTable;

typedef enum
{
    MENU_MESSAGE_TITLE_HERO_STATUS = 1,
    MENU_MESSAGE_TITLE_HERO_ABILITIES = 2,
    MENU_MESSAGE_TITLE_HERO_EQUIPMENT = 3,
    MENU_MESSAGE_TITLE_GUEST_HERO_STATUS = 4,
    MENU_MESSAGE_TITLE_GUEST_HERO_ABILITIES = 5,
    MENU_MESSAGE_TITLE_GUEST_HERO_EQUIPMENT = 6,
    MENU_MESSAGE_TITLE_GUEST_PARTNER_STATUS = 7,
    MENU_MESSAGE_TITLE_GUEST_PARTNER_ABILITIES = 8,
    MENU_MESSAGE_TITLE_GUEST_PARTNER_ATTRIBUTES = 9,
    MENU_MESSAGE_TITLE_PET_STATUS = 10,
    MENU_MESSAGE_TITLE_PET_ATTRIBUTES = 11,
    MENU_MESSAGE_TITLE_GOLEM_ATTRIBUTES = 12,
    MENU_MESSAGE_TITLE_GOLEM_STATUS = 13,
    MENU_MESSAGE_TITLE_GOLEM_LOGIC_BLOCKS = 14,
    MENU_MESSAGE_TITLE_ITEMS = 16,
    MENU_MESSAGE_TITLE_WEAPONS = 17,
    MENU_MESSAGE_TITLE_WEAPON_STATUS = 18,
    MENU_MESSAGE_TITLE_ITEMS_STATUS = 19,
    MENU_MESSAGE_TITLE_ARMOR = 20,
    MENU_MESSAGE_TITLE_ARMOR_STATUS = 21,
    MENU_MESSAGE_TITLE_INSTRUMENTS = 22,
    MENU_MESSAGE_TITLE_INSTRUMENT_STATUS = 23,
    MENU_MESSAGE_TITLE_ARTIFACTS = 24,
    MENU_MESSAGE_TITLE_GAME_ENVIRONMENT = 25,
    MENU_MESSAGE_TITLE_KEY_CONFIGURATION = 26,
    MENU_MESSAGE_TITLE_BROWSE_ALL = 27,
    MENU_MESSAGE_TITLE_BACKFIELD = 28,
    MENU_MESSAGE_TECHNIQUES_REMOVED = 29,
    MENU_MESSAGE_QUICK_ATTACKS = 58,
    MENU_MESSAGE_POWER_ATTACKS = 59,
    MENU_MESSAGE_EVENT_LIST = 60,
    MENU_MESSAGE_TECHNIQUE = 61,
    MENU_MESSAGE_MAGIC = 62,
    MENU_MESSAGE_EQUIP = 63,
    MENU_MESSAGE_STATUS = 64,
    MENU_MESSAGE_AUTO_SELECT = 65,
    MENU_MESSAGE_TOTAL_AUTO = 66,
    MENU_MESSAGE_REMOVE = 67,
    MENU_MESSAGE_ITEM_STATUS = 68,
    MENU_MESSAGE_TRASH = 69,
    MENU_MESSAGE_CHANGE = 72,
    MENU_MESSAGE_CANCEL = 73,
    MENU_MESSAGE_CANNOT_REMOVE_EQUIPMENT = 86,
    MENU_MESSAGE_EQUIPPED_ITEM_SUFFIX = 90,
    MENU_MESSAGE_NO_CONTROLLER_LINE1 = 101,
    MENU_MESSAGE_NO_CONTROLLER_LINE2 = 102
} MenuMessage;

typedef enum
{
    MENU_PAGE_WEAPONS = 0,
    MENU_PAGE_ARMOR = 1,
    MENU_PAGE_INSTRUMENTS = 2,
    MENU_PAGE_EVENTS = 3,
    MENU_PAGE_ITEMS = 4,
    MENU_PAGE_ARTIFACTS = 5,
    MENU_PAGE_MESSAGE = 6,
    MENU_PAGE_TWO_LINE_MESSAGE = 7
} MenuContentPage;

typedef enum
{
    MENU_ACTION_NONE = 0,
    MENU_ACTION_RENAME = 1,
    MENU_ACTION_OPTION_FIRST = 6,
    MENU_ACTION_SOUND_STEREO = 6,
    MENU_ACTION_SOUND_MONO = 7,
    MENU_ACTION_VIBRATION_ON = 8,
    MENU_ACTION_VIBRATION_OFF = 9,
    MENU_ACTION_GUEST_PORT_2_ON = 10,
    MENU_ACTION_GUEST_PORT_2_OFF = 11,
    MENU_ACTION_OPTION_END = 12
} MenuActionCode;

typedef enum
{
    MENU_HELP_FIRST = 0xF0,
    MENU_HELP_MONSTER_FAMILY = 0xF0,
    MENU_HELP_ARTIFACT = 0xF1,
    MENU_HELP_MAGIC_SHAPE = 0xF2,
    MENU_HELP_SPIRIT = 0xF3,
    MENU_HELP_MAGIC = 0xF4,
    MENU_HELP_ITEM_SPECIAL = 0xF5,
    MENU_HELP_MONSTER_TECHNIQUE = 0xF6,
    MENU_HELP_WEAPON_ATTACK = 0xF7,
    MENU_HELP_SKILL = 0xF8,
    MENU_HELP_TECHNIQUE = 0xF9,
    MENU_HELP_ABILITY = 0xFA,
    MENU_HELP_ITEM_FIRST = 0xFB
} MenuHelpSource;

#define MENU_NAME_PARTS_ARMOR_FIRST 16
#define MENU_NAME_PARTS_INSTRUMENT_FIRST 32
#define MENU_NAME_PARTS_MATERIAL_FIRST 36
#define MENU_TYPE_NAMES_ARMOR_FIRST FIELD_WEAPON_TYPE_COUNT
#define MENU_TYPE_NAMES_INSTRUMENT_FIRST 23
#define MENU_NODE_ANIM_FRAMES_MASK 0x000C

#define MENU_ICON_NONE 0xFF
#define MENU_ICON_R1_HINT 0x1E
#define MENU_ICON_EMPTY 0x21
#define MENU_ICON_SPIRIT_FIRST 0x35
#define MENU_ICON_STATUS_FIRST 0x3D
#define MENU_ICON_WEAPON_FIRST 0x45
#define MENU_ICON_ARMOR_FIRST 0x50
#define MENU_ICON_INSTRUMENT_FIRST 0x5C
#define MENU_ICON_SET 0x6A

typedef enum
{
    MENU_ICON_SELECT_R1_HINT_END = 0x02,
    MENU_ICON_SELECT_ARMOR_STATUS_FIRST = 0x02,
    MENU_ICON_SELECT_ARMOR_ELEMENT_FIRST = 0x0A,
    MENU_ICON_SELECT_ARMOR_SET_STATUS_FIRST = 0x12,
    MENU_ICON_SELECT_ARMOR_SET_ELEMENT_FIRST = 0x1A,
    MENU_ICON_SELECT_WEAPON_FLAG_FIRST = 0x22,
    MENU_ICON_SELECT_BUTTON_FIRST = 0x2A,
    MENU_ICON_SELECT_SLOT_FIRST = 0x32,
    MENU_ICON_SELECT_VIEWED_ITEM = 0x36,
    MENU_ICON_SELECT_ACTION_FIRST = 0x37,
    MENU_ICON_SELECT_ACTION_LAST = 0x54,
    MENU_ICON_SELECT_STEREO = 0x55,
    MENU_ICON_SELECT_MONO = 0x56,
    MENU_ICON_SELECT_VIBRATION_ON = 0x57,
    MENU_ICON_SELECT_VIBRATION_OFF = 0x58,
    MENU_ICON_SELECT_GUEST_PORT_2_ON = 0x59,
    MENU_ICON_SELECT_GUEST_PORT_2_OFF = 0x5A
} MenuIconSelector;

typedef enum
{
    MENU_SKILL_COMMAND_TECHNIQUE = 0,
    MENU_SKILL_COMMAND_MAGIC = 1,
    MENU_SKILL_COMMAND_REMOVE = 2,
    MENU_SKILL_COMMAND_STATUS = 3
} MenuSkillCommand;

typedef enum
{
    MENU_EQUIPMENT_COMMAND_EQUIP = 0,
    MENU_EQUIPMENT_COMMAND_AUTO_SELECT = 1,
    MENU_EQUIPMENT_COMMAND_TOTAL_AUTO = 2,
    MENU_EQUIPMENT_COMMAND_STATUS = 3,
    MENU_EQUIPMENT_COMMAND_REMOVE = 4
} MenuEquipmentCommand;

typedef enum
{
    MENU_CONTENT_KIND_NONE = 0,
    MENU_CONTENT_KIND_MESSAGE = 1,
    MENU_CONTENT_KIND_ICON = 2,
    MENU_CONTENT_KIND_ITEM_FIELD = 3,
    MENU_CONTENT_KIND_PARTY_FIELD = 4,
    MENU_CONTENT_KIND_SUBMENU = 5,
    MENU_CONTENT_KIND_GAME_FIELD = 6,
    MENU_CONTENT_KIND_ATTACK_LABEL = 7,
    MENU_CONTENT_KIND_ACTION = 15
} MenuContentKind;

typedef enum
{
    MENU_ITEM_FIELD_NAME = 0x01,
    MENU_ITEM_FIELD_TYPE_NAME = 0x02,
    MENU_ITEM_FIELD_BONUS_FIRST = 0x03,
    MENU_ITEM_FIELD_SPECIAL_FIRST = 0x0B,
    MENU_ITEM_FIELD_WEAPON_ATTACK_FIRST = 0x0E,
    MENU_ITEM_FIELD_WEAPON_POWER = 0x11,
    MENU_ITEM_FIELD_ARMOR_VALUE = 0x12,
    MENU_ITEM_FIELD_INSTRUMENT_MAGIC_NAME = 0x13,
    MENU_ITEM_FIELD_INSTRUMENT_SPIRIT_NAME = 0x14,
    MENU_ITEM_FIELD_INSTRUMENT_SHAPE_NAME = 0x15,
    MENU_ITEM_FIELD_INSTRUMENT_POWER = 0x16,
    MENU_ITEM_FIELD_SLOT_CHANGE_FIRST = 0x17,
    MENU_ITEM_FIELD_SLOT_CHANGE_SIGN_FIRST = 0x1B,
    MENU_ITEM_FIELD_MODIFIER_SIGN_FIRST = 0x1F,
    MENU_ITEM_FIELD_MODIFIER_FIRST = 0x27,
    MENU_ITEM_FIELD_MODIFIER_CHANGE_SIGN_FIRST = 0x2F,
    MENU_ITEM_FIELD_MODIFIER_CHANGE_FIRST = 0x37,
    MENU_ITEM_FIELD_ARMOR_VALUE_FIRST = 0x3F,
    MENU_ITEM_FIELD_ARMOR_TOTAL_CHANGE_SIGN_FIRST = 0x43,
    MENU_ITEM_FIELD_ARMOR_TOTAL_CHANGE_FIRST = 0x47
} MenuItemField;

typedef enum
{
    MENU_PARTY_FIELD_PORTRAIT = 0x01,
    MENU_PARTY_FIELD_NAME = 0x02,
    MENU_PARTY_FIELD_LEVEL = 0x03,
    MENU_PARTY_FIELD_CURRENT_HP = 0x04,
    MENU_PARTY_FIELD_MAXIMUM_HP = 0x05,
    MENU_PARTY_FIELD_EXPERIENCE = 0x06,
    MENU_PARTY_FIELD_STAT_FIRST = 0x07,
    MENU_PARTY_FIELD_WEAPON_NAME = 0x0F,
    MENU_PARTY_FIELD_WEAPON_ATTACK_FIRST = 0x10,
    MENU_PARTY_FIELD_WEAPON_POWER = 0x13,
    MENU_PARTY_FIELD_ARMOR_NAME_FIRST = 0x14,
    MENU_PARTY_FIELD_COMMAND_NAME_FIRST = 0x17,
    MENU_PARTY_FIELD_NEXT_LEVEL_EXPERIENCE = 0x19,
    MENU_PARTY_FIELD_SKILL_NAME_FIRST = 0x1B,
    MENU_PARTY_FIELD_MONSTER_NAME = 0x1F,
    MENU_PARTY_FIELD_ARMOR_TOTAL_FIRST = 0x20,
    MENU_PARTY_FIELD_WEAPON_POWER_CHANGE_SIGN = 0x3F,
    MENU_PARTY_FIELD_ARMOR_TOTAL_CHANGE_SIGN_FIRST = 0x40,
    MENU_PARTY_FIELD_WEAPON_POWER_CHANGE = 0x44,
    MENU_PARTY_FIELD_ARMOR_TOTAL_CHANGE_FIRST = 0x45,
    MENU_PARTY_FIELD_COMPANION_ABILITY_NAME = 0x49,
    MENU_PARTY_FIELD_GOLEM_CLASS_NAME = 0x4A,
    MENU_PARTY_FIELD_GOLEM_GRID_SIZE = 0x4B,
    MENU_PARTY_FIELD_GOLEM_FAILURE_RATE = 0x4C,
    MENU_PARTY_FIELD_GOLEM_PALETTE_NAME = 0x4D,
    MENU_PARTY_FIELD_WEAPON_BONUS_FIRST = 0x4E
} MenuPartyField;

typedef enum
{
    MENU_GAME_FIELD_MONEY = 1,
    MENU_GAME_FIELD_PLAY_TIME = 2
} MenuGameField;

typedef enum
{
    MENU_ATTACK_LABEL_QUICK = 1,
    MENU_ATTACK_LABEL_POWER = 2
} MenuAttackLabel;

typedef struct
{
    u32 table_count;
    s32 table_offsets[MENU_TEXT_TABLE_COUNT];
} MenuTextResources;

typedef struct
{
    u8 label_id;
    u8 layout_frames_remaining;

    union
    {
        u16 word;

        struct
        {
            u8 flags;
            u8 parent_idx;
        } s;

        struct
        {
            u16 active : 1;
            u16 expanded : 1;
            u16 anim_frames : 2;
            u16 char_slot : 2;
            u16 depth : 2;
            u16 parent_idx : 8;
        } bits;
    } u2;

    u8 icon_id;
    u8 content_id;

    union
    {
        u16 nav_x_packed;

        struct
        {
            u8 self_idx;
            u8 nav_x;
        } s;
    } idx_nav;

    union
    {
        u16 nav_y_packed;

        struct
        {
            u8 nav_y_hi;
            u8 layout_x_y0;
        } s;
    } u8_u;

    union
    {
        u16 layout_child_packed;

        struct
        {
            u8 layout_y_hi;
            u8 children[MENU_MAX_CHILDREN];
            u8 unused_0f;
        } s;
    } layout;
} MenuNode;

typedef struct
{
    u8 label_id;
    u8 layout_frames_remaining;
    u16 flags_and_parent;
    u8 icon_id;
    u8 content_id;
    u32 self_idx : 8;
    u32 nav_x : 7;
    u32 nav_y : 9;
    u32 layout_x : 7;
    u32 layout_y : 9;
    u8 children[MENU_MAX_CHILDREN];
    u8 unused_0f;
} __attribute__((packed, aligned(2))) MenuNodeCoordinateView;

typedef struct
{
    u16 packed_x;
    u8 y;
    u8 action_type;
    u8 params[4];
} MenuContentItem;

typedef struct
{
    u16 x : 9;
    u16 action_index : 3;
    u16 type : 4;
    u8 y;
    u8 pad[5];
} MenuContentItemBits;

typedef struct
{
    u16 uv;
    u16 _pad2;
    u32 packed_xy;
    u32 packed_wh;
} MenuGridSpriteDef;

typedef struct
{
    u8 padding0[0x90];
    u8 small_motor_command;
    u8 padding91;
    u8 large_motor_command;
    u8 padding93[0x1B];
} MenuControllerActuatorPort;

typedef struct
{
    MenuControllerActuatorPort ports[2];
} MenuControllerActuatorState;

typedef enum
{
    MENU_CURSOR_MODE_NODE_TREE = 0,
    MENU_CURSOR_MODE_CONTENT = 1,
    MENU_CURSOR_MODE_CONTENT_EXIT = 2,
} MenuCursorMode;

typedef struct
{
    u8 x;
    u8 y;
    u8 pad2;
    u8 selected_idx;
} MenuButtonSwapMarker;

typedef struct
{
    u8 u_coord;
    u8 v_coord;
    u8 w;
    u8 h;
} MenuIconSpriteInfo;

typedef struct
{
    u8 active;
    u8 index;
    u8 anim_frame;
    u8 has_title;
    MenuListNavigation navigation;
    u16 base_x;
    u16 base_y;
    s16 viewport_w;
    s16 viewport_h;
    u16 scroll_x;
    u16 scroll_y;
    s16 target_x;
    s16 target_y;
    u8 lerp_steps;
} ScrollListState;

typedef union
{
    u8 packed;

    struct
    {
        u32 index : 7;
        u32 stored_name : 1;
    } __attribute__((packed)) fields;
} MenuNameSelection;

void menu_init(void);
void menu_tick(RenderContext* render_ctx);
void menu_build_grid(RenderContext* render_ctx);
void menu_set_active_node(void);
void menu_snap_view_to_cursor(void);
void menu_reset_content_view(void);
void menu_init_item_nav_entries(s32 count);
void menu_update_active_slot(void);
void menu_update_slots(RenderContext* render_ctx);
void menu_draw_window_transition(RenderContext* render_ctx, MenuSlot* slot, s32 cursor_enable);
void menu_draw_window(MenuSlot* slot, RenderContext* render_ctx, MenuRect* rect, Vec2s* view_origin, s32 cursor_enable);
SPRT* menu_emit_corner(SPRT* sprite, OT_TYPE* ot_entry, s32 x, s32 y, u32 uv);
SPRT* menu_fill_window_interior(SPRT* sprite, OT_TYPE* ot_entry, const MenuRectU16* rect, u32 uv);
u_long* menu_build_h_edge(u_long* packet_cursor, OT_TYPE* ot_entry, const MenuRectU16* rect, s32 texture_origin);
u_long* menu_build_v_edge(u_long* packet_cursor, OT_TYPE* ot_entry, const MenuRectU16* rect, s32 texture_origin);
u8* menu_draw_frame(u8* packet_cursor, OT_TYPE* ot_entry, s32 frame_parity, s32 allow_input);
void* menu_draw_scene_content(void* packet_cursor, OT_TYPE* ot_entry);
void* menu_draw_active_node_cursor(void* packet_cursor, OT_TYPE* ot_entry, s32 draw_label);
void* menu_draw_content_cursor(void* prim_buf, OT_TYPE* ot, s32 draw_label);
void menu_handle_node_input(void);
u32 menu_step_item_selection(s32 step);

void menu_upload_tim(const TimUploadDestination* layout);
void* menu_draw_clamped_number(OT_TYPE* ot_entry, void* packet_cursor, s32 value, s32 text_color, Vec2s* origin, s32 flags);

void* menu_ability_list_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void* menu_event_list_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void* menu_item_list_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void* menu_artifact_list_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void* menu_message_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void* menu_two_line_message_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void* menu_inventory_list_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, s32 active);
void* menu_equipment_action_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void* menu_skill_action_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);

void* menu_emit_cursor(void* prim_buf, OT_TYPE* ot, s32 x, s32 y, s32 active);
void* menu_emit_draw_mode_primitive(DR_TPAGE* draw_mode, OT_TYPE* ot);
void* menu_emit_slot_scroll_arrows(SPRT* sprite, OT_TYPE* ot_entry, MenuSlot* slot);
void* menu_emit_tree_scroll_arrows(SPRT* sprite, OT_TYPE* ot_entry);

void* menu_emit_swap_marker(void*, OT_TYPE*, s16, s16);
s32 menu_item_differs_from_bare_hands(const FieldItemRecord*);

void* menu_draw_node_tree(void* prim_buf, OT_TYPE* ot);
void* menu_draw_node_recursive(s32 node_index, void* prim_buf, OT_TYPE* ot);

void* menu_emit_icon_sprite(void*, OT_TYPE*, s32, s32, s32, s32, s32, s32, s32);

void scroll_list_update_target(ScrollListState*, u32*);

void* menu_item_action_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void* menu_equipment_compare_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, int active);
void field_copy_inventory_record();
void field_compact_inventory();
FieldItemRecord* field_find_free_inventory_record(void);
void* menu_special_technique_list_callback(OT_TYPE* ot, ScrollListState* state, void* prim_buf, Vec2s* view_origin, s32 active);
void menu_swap_item_records(FieldItemRecord*, FieldItemRecord*);

void menu_play_se(s32 sound_id, s32 volume);

s32 menu_stage_best_weapon(void);
s32 menu_stage_best_armor(void);

void* scroll_list_draw(void* prim_buf, OT_TYPE* ot, ScrollListState* state, u32* entries, Vec2s* view_origin, int active);

FieldItemRecord* menu_find_best_weapon(void);
FieldItemRecord* menu_find_best_armor(void);
void menu_open_content_page(u32 content_id);

s32 menu_build_inventory_nav_entries(s32 item_kind);
s32 menu_build_event_nav_entries(void);
s32 menu_build_item_nav_entries(void);
s32 menu_build_artifact_nav_entries(void);

extern s32 g_playtime_vsync_origin;
extern u8 g_menu_default_content_count_minus_one;
extern u8 g_menu_content_item_counts[];
extern MenuContentItem g_menu_default_content_items;
extern u8 g_menu_action_icon_ids[];
extern u8 g_menu_element_icon_ids[];
extern u8 g_menu_button_icon_ids[];
extern FieldItemRecord* g_menu_shown_armor;
extern FieldItemRecord* g_menu_shown_equipment_copy;
extern FieldItemRecord* g_menu_shown_equipment;

extern u8* g_menu_help_text;
extern s32 g_menu_cursor_mode;
extern s32 g_menu_draw_early_out;
extern RenderContext* g_menu_draw_buf_base;
extern s32 g_menu_display_delay;
extern s32 g_menu_suppress_cursor;
extern s32 g_menu_scene_type;

extern MenuNode g_menu_nodes[0x2C];
extern u8 g_menu_prev_node;
extern s32 g_menu_content_ready;

extern FieldItemRecord* g_item_slot_data[FIELD_EQUIPMENT_SLOT_COUNT];
extern u8 g_item_slot_flags[FIELD_EQUIPMENT_SLOT_COUNT];

extern FieldItemRecord* g_menu_item_ptr;
extern FieldItemRecord* g_menu_weapon_item;
extern FieldItemRecord* g_menu_armor_item;
extern FieldItemRecord* g_menu_instrument_item;
extern FieldItemRecord* g_menu_active_equipped_item;
extern FieldItemRecord* g_menu_saved_category0_item;
extern FieldItemRecord* g_menu_saved_category1_item;
extern s32 g_menu_item_nav_entries[];
extern FieldItemRecord* g_menu_equipment_base;
extern s32 g_menu_content_height;
extern s32 g_menu_scroll_pos;
extern s32 g_menu_redraw_state;
extern s32 g_menu_active_node;
extern FieldItemRecord* g_menu_saved_equipment_item;
extern u8 g_menu_init_content_id;

extern s8 g_menu_equipment_conflicts;
extern u8 g_menu_companion_node;

extern s32 g_menu_nav_count;
extern s32 g_menu_nav_nodes[];
extern s32 g_content_view_y;
extern s32 g_menu_load_request;
extern s32 g_menu_transition_code;
extern s32 g_content_cursor_x;
extern s32 g_menu_hit_item_idx;
extern s32 g_content_view_x;
extern s32 g_content_cursor_y;

extern struct
{
    s16 x;
    s16 y;
} g_menu_default_view_pos;

extern MenuContentItem* g_menu_content_table[];
extern s32 g_menu_layout_end;

extern s32 g_menu_char_slot;

extern s32 g_menu_page_count;
extern s32 g_menu_active_subtype;
extern u32 g_menu_scroll_nav_entries[];

extern MenuButtonSwapMarker g_menu_button_swap_marker;
extern u8 g_menu_content_group_ids[];
extern u8 g_menu_content_action_codes[][8];
extern void* g_menu_message_line1;
extern void* g_menu_message_line2;

extern u8 g_field_weapon_type_conflicts[];
extern u8 g_field_armor_type_conflicts[];

extern s8 g_field_stat_modifier_values[];

extern s32 g_menu_help_skill_selection;

extern MenuIconSpriteInfo g_menu_icon_sprite_defs[];
extern u8 g_menu_icon_clut_codes[];

extern u8 g_menu_cursor_icon_ids[];

extern u8 g_menu_list_page_label_ids[];
extern u8 g_menu_technique_page_label_ids[];
extern u8 g_menu_item_description_buffer[];
extern s32 g_menu_inventory_index;
extern s32 g_menu_active_item_category;
extern s32 g_menu_pending_item_row;

extern u8 g_menu_armor_page_label_ids[];
extern FieldItemRecord g_field_bare_hands_item;
extern u32 g_field_bare_hands_info;

/** @brief Mark every menu window slot as free. */
static inline void menu_clear_slots(void)
{
    s32 slot_index;

    for (slot_index = MENU_SLOT_COUNT - 1; slot_index >= 0; slot_index--)
    {
        g_menu_slots[slot_index].active = 0;
    }
}

/**
 * @brief Link the first @p count entries of g_menu_item_nav_entries into a circular list.
 * @param count Number of navigation entries to initialize.
 */
static inline void menu_relink_item_nav(s32 count)
{
    s32 entry_index;
    s32 previous_index;
    s32 link;
    s32 entry_with_position;
    s32 entry_with_previous;
    s32 next_index;
    s32 has_next;

    for (entry_index = 0; entry_index < count; entry_index++)
    {
        s32 packed_entry = g_menu_item_nav_entries[entry_index];

        previous_index = count - 1;
        link = packed_entry & ~MENU_ITEM_NAV_POSITION_MASK;
        link = link | ((entry_index * MENU_LIST_ROW_HEIGHT) & MENU_ITEM_NAV_POSITION_MASK);
        entry_with_position = link;
        g_menu_item_nav_entries[entry_index] = entry_with_position;

        if ((entry_index - 1) >= 0)
        {
            previous_index = entry_index - 1;
        }

        entry_with_previous = entry_with_position & MENU_ITEM_NAV_PREVIOUS_CLEAR_MASK;
        entry_with_previous = entry_with_previous | ((previous_index & MENU_ITEM_NAV_INDEX_MASK) << MENU_ITEM_NAV_PREVIOUS_SHIFT);
        g_menu_item_nav_entries[entry_index] = entry_with_previous;
        next_index = entry_index + 1;
        has_next = next_index < count;
        link = 0;

        if (has_next != 0)
        {
            link = next_index;
        }

        g_menu_item_nav_entries[entry_index] = (entry_with_previous & MENU_ITEM_NAV_NEXT_CLEAR_MASK) | (link << MENU_ITEM_NAV_NEXT_SHIFT);
    }
}

/** @brief Build circular navigation links for a two-entry item list. */
static inline void menu_relink_pair(void)
{
    s32 index;
    s32 previous_index;
    s32 next_index;
    s32 has_next;
    s32 link;
    s32 entry_with_previous;
    index = 0;

    while (index < 2)
    {
        s32 entry = g_menu_item_nav_entries[index];
        s32 entry_with_position;
        previous_index = 1;
        link = entry & ~MENU_ITEM_NAV_POSITION_MASK;
        link = link | ((index * MENU_LIST_ROW_HEIGHT) & MENU_ITEM_NAV_POSITION_MASK);
        entry_with_position = link;
        g_menu_item_nav_entries[index] = entry_with_position;

        if ((index - 1) >= 0)
        {
            previous_index = index - 1;
        }

        entry_with_previous = entry_with_position & MENU_ITEM_NAV_PREVIOUS_CLEAR_MASK;
        entry_with_previous = entry_with_previous | ((previous_index & MENU_ITEM_NAV_INDEX_MASK) << MENU_ITEM_NAV_PREVIOUS_SHIFT);
        g_menu_item_nav_entries[index] = entry_with_previous;
        next_index = index + 1;
        has_next = next_index < 2;
        link = 0;

        if (has_next != 0)
        {
            link = next_index;
        }

        g_menu_item_nav_entries[index] = (entry_with_previous & MENU_ITEM_NAV_NEXT_CLEAR_MASK) | (link << MENU_ITEM_NAV_NEXT_SHIFT);
        index = next_index;
    }
}

/**
 * @brief Locate an equipped item belonging to the active character.
 * @param context Shared save-state context.
 * @param equipment_slot Index within the character's equipment array.
 * @return Address of the equipped item record.
 */
static inline FieldItemRecord* menu_equipped_item(SavedGameLayout* context, s32 equipment_slot)
{
    s32 offset = equipment_slot * MENU_ITEM_RECORD_SIZE;
    offset += g_menu_char_slot * MENU_CHARACTER_BLOCK_SIZE;
    return (FieldItemRecord*)((u8*)context + offset + MENU_CHARACTER_RECORD_OFFSET + MENU_CHARACTER_ITEMS_OFFSET);
}

/**
 * @brief Locate one character record in the shared context.
 * @param context Shared save-state context.
 * @param character_slot Character record index.
 * @return Character record, including its equipped items.
 */
static inline FieldCharacterRecord* menu_character_record(SavedGameLayout* context, s32 character_slot)
{
    return (FieldCharacterRecord*)((u8*)context + (character_slot * MENU_CHARACTER_BLOCK_SIZE + MENU_CHARACTER_RECORD_OFFSET));
}

/**
 * @brief Draw a content label at the menu's fixed help-text position.
 * @param packet_cursor First free primitive byte.
 * @param ot Ordering-table entry receiving the text primitives.
 * @param text Encoded text to draw.
 * @return First free primitive byte after the label.
 */
static inline void* menu_emit_content_label(void* packet_cursor, OT_TYPE* ot, u8* text)
{
    return field_draw_text(packet_cursor, ot, text, 1, 0xA0, 0xCA, FIELD_TEXT_ALIGN_CENTER);
}

/**
 * @brief Concatenate two encoded menu strings and write their terminator.
 * @param destination Buffer large enough for both strings and the terminator.
 * @param first First null-terminated encoded string.
 * @param second Second null-terminated encoded string.
 * @note Bytes 0x19-0x1F introduce a second byte, which may be zero.
 */
static inline void menu_copy_encoded_pair(u8* destination, const u8* first, const u8* second)
{
    while (1)
    {
        u8 character = *first;

        if (character == 0)
        {
            break;
        }

        if ((u8)(character - MENU_TEXT_PREFIX_FIRST) < MENU_TEXT_PREFIX_COUNT)
        {
            *destination++ = character;
            first++;
            *destination++ = *first++;
        }
        else
        {
            *destination++ = character;
            first++;
        }
    }

    while (1)
    {
        u8 character = *second;

        if (character == 0)
        {
            break;
        }

        if ((u8)(character - MENU_TEXT_PREFIX_FIRST) < MENU_TEXT_PREFIX_COUNT)
        {
            *destination++ = character;
            second++;
            *destination++ = *second++;
        }
        else
        {
            *destination++ = character;
            second++;
        }
    }

    *destination = 0;
}

/**
 * @brief Return one text table from the active resource directory.
 * @param table Resource-directory index.
 * @return Table base used by its relative string offsets.
 */
static inline u8* menu_text_table_base(s32 table)
{
    u8* resources = (u8*)g_menu_state_ptr;
    return resources + ((MenuTextResources*)resources)->table_offsets[table];
}

/**
 * @brief Resolve one table-relative string offset.
 * @param base Text table beginning with halfword offsets.
 * @param index String-offset index.
 * @return Encoded string at that offset.
 */
static inline u8* menu_text_entry(void* base, s32 index)
{
    return (u8*)base + ((u16*)base)[index];
}

/**
 * @brief Show a one-line message window in place of the open windows.
 * @param message MENU_TEXT_MESSAGES entry to show.
 */
static inline void menu_show_message(s32 message)
{
    g_menu_message_line1 = menu_text_entry(menu_text_table_base(MENU_TEXT_MESSAGES), message);
    menu_clear_slots();
    menu_open_content_page(MENU_PAGE_MESSAGE);
}

/**
 * @brief Locate image data at a word-aligned offset in the upload buffer.
 * @param buffer Word-aligned base of the image upload buffer.
 * @param byte_offset Byte offset, rounded down to a four-byte boundary.
 * @return Source words for LoadImage.
 */
static inline u_long* menu_image_upload_source(void* buffer, s32 byte_offset)
{
    return (u_long*)(((byte_offset >> 2) << 2) + (uintptr_t)buffer);
}

/**
 * @brief Convert a packed menu icon palette code to a Psy-Q CLUT id.
 * @param code Palette column in the low nibble, row above VRAM_CLUT_Y in the high nibble.
 * @return The CLUT id.
 */
static inline u16 menu_icon_clut(u8 code)
{
    return lom_gpu_clut((code & 0xF) << 4, (code >> 4) + VRAM_CLUT_Y);
}

/**
 * @brief Read the seven-bit navigation column from a packed position word.
 * @param packed Position word containing the column in bits 14:8.
 * @return Navigation column.
 */
static inline s32 menu_nav_x(u16 packed)
{
    return (packed >> 8) & (MENU_NAV_X_MASK >> 8);
}

/**
 * @brief Address of the FIELD UI string an offset entry points at.
 * @param entry Offset entry, one of the g_field_ui_text_* symbols.
 * @param index FIELD_UI_TEXT_* index of @p entry.
 * @return The entry's string.
 */
static inline u8* menu_shared_text_entry(const FieldUiTextOffset* entry, s32 index)
{
    uintptr_t string_page_base = (entry->high << 8) + (uintptr_t)(entry - index);
    return (u8*)(entry->low + string_page_base);
}

/**
 * @brief Copy the label for a value's sign and append its terminator.
 * @param buffer Destination text buffer.
 * @param value Value whose sign picks the label (zero counts as positive).
 */
static inline void menu_copy_sign_label(u8* buffer, s32 value)
{
    u8* write_cursor = buffer;
    u8* source;

    if (value >= 0)
    {
        source = menu_shared_text_entry(&g_field_ui_text_plus, FIELD_UI_TEXT_PLUS);
        field_copy_name(write_cursor, source);
    }
    else
    {
        source = menu_shared_text_entry(&g_field_ui_text_minus, FIELD_UI_TEXT_MINUS);
        field_copy_name(write_cursor, source);
    }

    write_cursor += field_name_byte_length(source);
    *write_cursor = 0;
}

#endif
