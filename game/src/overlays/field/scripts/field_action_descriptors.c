#include "common.h"

#include "../internal/field_actor.h"
#include "../internal/field_calls.h"
#include "../internal/field_records.h"
#include "main/audio/game_audio.h"

#define STATUS_KIND_MASK 0xFC00

#define STATUS_KIND_BITS(kind) ((kind) << 10)

#define STATUS_KIND_COMPANION 3

#define STATUS_KIND_COMPANION_B 4

#define COMPANION_ACTION_PAGE 2

#define MONSTER_ACTION_NONE_A 23
#define MONSTER_ACTION_NONE_B 24

enum
{
    ACTION_TABLE_WEAPON,
    ACTION_TABLE_WEAPON_TYPE,
    ACTION_TABLE_ITEM,
    ACTION_TABLE_COMMAND
};

#define WEAPON_TYPE_ACTION_COUNT 24

#define ITEM_ROW_ACTION_COUNT 14

#define FIRST_COMMAND_ACTION 7

enum
{
    ACTION_COMMAND_0 = 0,
    ACTION_COMMAND_1 = 1,
    ACTION_WEAPON_2 = 2,
    ACTION_WEAPON_3 = 3,
    ACTION_SKILL_0 = 4,
    ACTION_SKILL_1 = 5,
    ACTION_SKILL_2 = 6,
    ACTION_SKILL_3 = 7,
    ACTION_WEAPON_8 = 8,
    ACTION_WEAPON_9 = 9,
    ACTION_WEAPON_10 = 10
};

#define SKILL_SLOT_ITEM 0x80

typedef struct ItemActionRow
{
    FieldActionDescriptor actions[ITEM_ROW_ACTION_COUNT];
} ItemActionRow;

static FieldActionDescriptor* field_command_action_descriptor(s32 command);

/**
 * @brief Return one table of the action descriptor bank.
 * @param table ACTION_TABLE_* index.
 * @return First descriptor of the table.
 */
static inline FieldActionDescriptor* action_table(s32 table)
{
    return (FieldActionDescriptor*)((u8*)g_field_action_bank + g_field_action_bank->table_offsets[table]);
}

/**
 * @brief Select the descriptor of the attacker's current action and reset the action power.
 * @return The descriptor, or NULL when the action has none.
 * @note Weapon action bytes are derived.weapon.stats[0] to [4] of the attacker's weapon.
 */
FieldActionDescriptor* field_select_action_descriptor(void)
{
    s32 weapon_action;
    s32 action_id;
    s32 kind;
    u32 selector;
    u32 slot;
    FieldItemRecord* weapon;
    FieldItemRecord* item;
    FieldActionDescriptor* descriptor;
    FieldStatusRecord* attacker;
    FieldBattleAction* action;
    FieldActorTemplate* template;
    FieldBattleContext* ctx;

    g_field_battle->power = g_field_battle->attacker->power;
    g_field_battle->power_flags = 0;
    attacker = g_field_battle->attacker;
    kind = attacker->meta.packed & STATUS_KIND_MASK;

    if (kind == STATUS_KIND_BITS(FIELD_STATUS_KIND_MONSTER))
    {
        action = g_field_battle->action;
        template = attacker->template;
        action_id = action->action_id;

        if (action_id < template->action_count)
        {
            descriptor = (FieldActionDescriptor*)template->actions[action_id];
        }
        else if ((action_id != MONSTER_ACTION_NONE_A) && (action_id != MONSTER_ACTION_NONE_B))
        {
            record_game_diagnostic(DIAG_ERROR, DIAG_BAD_MONSTER_ACTION, action->attacker_id, action_id);
            descriptor = NULL;
        }
        else
        {
            descriptor = NULL;
        }
    }
    else if ((kind == STATUS_KIND_BITS(STATUS_KIND_COMPANION)) || (kind == STATUS_KIND_BITS(STATUS_KIND_COMPANION_B)))
    {
        descriptor = field_get_party_action(COMPANION_ACTION_PAGE, g_field_battle->action->action_id);
    }
    else
    {
        weapon = &g_field_game_state->characters[attacker->meta.bytes.id].equipment[0];
        g_field_battle->power_flags = weapon->status_flags;
        selector = g_field_battle->action->action_id;

        switch (selector)
        {
        case ACTION_COMMAND_0:
        case ACTION_COMMAND_1:
            descriptor = field_command_action_descriptor(
                g_field_game_state->characters[g_field_battle->attacker->meta.bytes.id].info.actions.commands[g_field_battle->action->action_id]);
            break;
        case ACTION_WEAPON_2:
            descriptor = &action_table(ACTION_TABLE_WEAPON)[weapon->derived.weapon.stats[0]];
            descriptor += g_field_battle->action->param;
            break;
        case ACTION_WEAPON_3:
            descriptor = &action_table(ACTION_TABLE_WEAPON)[weapon->derived.weapon.stats[g_field_battle->action->action_id - 2]];
            break;
        case ACTION_SKILL_0:
        case ACTION_SKILL_1:
        case ACTION_SKILL_2:
        case ACTION_SKILL_3:
            ctx = g_field_battle;
            slot = g_field_game_state->characters[ctx->attacker->meta.bytes.id].info.actions.skills[ctx->action->action_id - ACTION_SKILL_0];

            if (slot < SKILL_SLOT_ITEM)
            {
                s32 index;

                index = weapon->info.bits.item_type * WEAPON_TYPE_ACTION_COUNT + slot;
                descriptor = &action_table(ACTION_TABLE_WEAPON_TYPE)[index];
            }
            else
            {
                item = &g_field_game_state->characters[ctx->attacker->meta.bytes.id].equipment[(slot + FIELD_EQUIPMENT_SLOT_COUNT) & (SKILL_SLOT_ITEM - 1)];
                slot = item->derived.bytes[2];
                ctx->power_flags = 0;
                ctx->power = slot;
                descriptor = &((ItemActionRow*)action_table(ACTION_TABLE_ITEM))[item->derived.bytes[0]].actions[item->derived.bytes[1]];
            }

            break;
        case ACTION_WEAPON_8:
        case ACTION_WEAPON_9:
        case ACTION_WEAPON_10:
            weapon_action = g_field_battle->action->action_id;
            descriptor = &action_table(ACTION_TABLE_WEAPON)[weapon->derived.weapon.stats[weapon_action - 6]];

            if (weapon_action == 18 || weapon_action == 19)
            {
                g_field_battle->attacker->counter--;
            }

            break;
        default:
            descriptor = &action_table(ACTION_TABLE_COMMAND)[selector] - FIRST_COMMAND_ACTION;
            break;
        }
    }

    return descriptor;
}

/**
 * @brief Return the ACTION_TABLE_COMMAND descriptor of a character command.
 * @param command Command code from info.actions.commands.
 * @return The descriptor, or NULL for Bash and for unknown commands.
 */
static FieldActionDescriptor* field_command_action_descriptor(s32 command)
{
    FieldActionDescriptor* descriptor;

    descriptor = action_table(ACTION_TABLE_COMMAND);

    switch (command)
    {
    case FIELD_ABILITY_CHEER:
        field_battle_run_party_event(g_field_battle->attacker->meta.bytes.id);
        descriptor += 16;
        break;
    case FIELD_ABILITY_SPIN:
        descriptor += 15;
        g_field_battle->attacker->counter--;
        break;
    case FIELD_ABILITY_PUSH:
        break;
    case FIELD_ABILITY_WHIRL:
        descriptor++;
        break;
    case FIELD_ABILITY_BASH:
        descriptor = NULL;
        break;
    case FIELD_ABILITY_TACKLE:
        descriptor += 2;
        break;
    case FIELD_ABILITY_TOSS:
        descriptor += 3;
        break;
    case FIELD_ABILITY_SLIDE:
        descriptor += 4;
        break;
    case FIELD_ABILITY_FLIP_KICK:
        descriptor += 14;
        break;
    default:
        record_game_diagnostic(DIAG_ERROR, DIAG_BAD_COMMAND, command, -1);
        return NULL;
    }

    return descriptor;
}
