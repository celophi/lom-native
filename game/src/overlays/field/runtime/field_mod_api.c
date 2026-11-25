#include "common.h"
#include "lom_native_mod_game.h"
#include "overlays/field/field_actor_records.h"
#include "overlays/field/field_player_records.h"
#include "../internal/field_calls.h"
#include "../internal/field_records.h"
#include "../internal/field_state_ops.h"

/**
 * @brief Check a party slot has someone in it.
 * @param slot The slot.
 * @return 1 when it does, 0 when it's empty or out of range.
 * @note Native: for mods' scripts (lom_native_mod_game.h).
 */
static s32 field_mod_slot_active(s32 slot)
{
    return slot >= 0 && slot < FIELD_PARTY_COUNT && (g_field_player_records[slot].head.bytes.flags & FIELD_PLAYER_ACTIVE) != 0;
}

/**
 * @brief Read the party's lucre.
 * @return The lucre.
 * @note Native: for mods' scripts (lom_native_mod_game.h).
 */
s32 field_mod_money(void)
{
    return (s32)g_field_game_state->money;
}

/**
 * @brief Give the party lucre through field_add_money.
 * @param amount How much; 0 or less does nothing.
 * @return 1 when it added any, 0 otherwise.
 * @note Native: for mods' scripts (lom_native_mod_game.h).
 */
s32 field_mod_add_money(s32 amount)
{
    if (amount <= 0)
    {
        return 0;
    }

    field_add_money(0, amount);
    return 1;
}

/**
 * @brief Take lucre from the party through field_spend_money.
 * @param amount How much; 0 or less does nothing.
 * @return 1 when it was spent, 0 otherwise.
 * @note Native: for mods' scripts (lom_native_mod_game.h).
 */
s32 field_mod_spend_money(s32 amount)
{
    if (amount <= 0)
    {
        return 0;
    }

    return field_spend_money((u32)amount) != 0;
}

/**
 * @brief Read a party member's hit points.
 * @param slot The party slot.
 * @param hp Set to their hit points.
 * @param maximum_hp Set to their maximum.
 * @return 1 when the slot has someone in it, 0 otherwise.
 * @note Native: for mods' scripts (lom_native_mod_game.h).
 */
s32 field_mod_party_member(s32 slot, s32* hp, s32* maximum_hp)
{
    if (!field_mod_slot_active(slot))
    {
        return 0;
    }

    *hp = (s32)g_field_object_states[slot].current_hp.word;
    *maximum_hp = (s32)g_field_object_states[slot].maximum_hp;
    return 1;
}

/**
 * @brief Restore a party member's hit points through field_heal_status.
 * @param slot The party slot.
 * @param amount How many; 0 or less does nothing.
 * @return 1 when it healed them, 0 when the slot is empty, they're down or the
 *         amount wasn't positive.
 * @note Native: for mods' scripts (lom_native_mod_game.h).
 */
s32 field_mod_heal(s32 slot, s32 amount)
{
    if (amount <= 0 || !field_mod_slot_active(slot) || g_field_object_states[slot].current_hp.word == 0)
    {
        return 0;
    }

    field_heal_status(&g_field_object_states[slot], amount);
    return 1;
}
