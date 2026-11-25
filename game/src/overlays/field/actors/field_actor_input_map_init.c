#include "common.h"
#include "common/saved_game.h"
#include "../internal/field_calls.h"
#include "../internal/field_actor_tables.h"

#define FIELD_WEAPON_ACTION_COUNT 5

extern u8 g_field_weapon_action_slots[FIELD_WEAPON_ACTION_COUNT];
extern u8 g_field_weapon_action_default_params[FIELD_WEAPON_ACTION_COUNT];
extern u8 g_field_weapon_action_blunt_params[FIELD_WEAPON_ACTION_COUNT];
extern u8 g_field_weapon_action_bow_params[FIELD_WEAPON_ACTION_COUNT];

/**
 * @brief Reset both action command maps and the player action slots to their defaults.
 */
void field_reset_action_command_maps(void)
{
    field_reset_action_command_map(0);
    field_reset_action_command_map(1);
}

/**
 * @brief Set the weapon-dependent action parameters of a player from its weapon type.
 * @param player_index Player record and resource action row to update.
 */
void field_apply_weapon_action_params(s32 player_index)
{
    s32 i;

    for (i = 0; i < FIELD_WEAPON_ACTION_COUNT; i++)
    {
        switch (g_field_player_records[player_index].head.bytes.weapon_type)
        {
        case FIELD_WEAPON_HAMMER:
        case FIELD_WEAPON_STAFF:
        case FIELD_WEAPON_GLOVE:
        case FIELD_WEAPON_FLAIL:
            g_field_resource_actions[player_index].slots[g_field_weapon_action_slots[i]].parameter = g_field_weapon_action_blunt_params[i];
            break;
        case FIELD_WEAPON_BOW:
            g_field_resource_actions[player_index].slots[g_field_weapon_action_slots[i]].parameter = g_field_weapon_action_bow_params[i];
            break;
        default:
            g_field_resource_actions[player_index].slots[g_field_weapon_action_slots[i]].parameter = g_field_weapon_action_default_params[i];
            break;
        }
    }
}
