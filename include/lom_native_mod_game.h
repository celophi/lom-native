#ifndef LOM_NATIVE_MOD_GAME_H
#define LOM_NATIVE_MOD_GAME_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Read the party's lucre. FIELD has to be running.
     * @return The lucre.
     */
    int field_mod_money(void);

    /**
     * @brief Give the party lucre, through the game's own field_add_money, so it
     *        stops at the game's maximum. FIELD has to be running.
     * @param amount How much; 0 or less does nothing.
     * @return 1 when it added any, 0 when the amount wasn't positive.
     */
    int field_mod_add_money(int amount);

    /**
     * @brief Take lucre from the party, through the game's own
     *        field_spend_money, which only spends when the party has more than
     *        the amount. FIELD has to be running.
     * @param amount How much; 0 or less does nothing.
     * @return 1 when it was spent, 0 when it wasn't.
     */
    int field_mod_spend_money(int amount);

    /**
     * @brief Read a party member's hit points. FIELD has to be running.
     * @param slot The party slot, 0 to 2.
     * @param hp Set to their hit points.
     * @param maximum_hp Set to their maximum hit points.
     * @return 1 when the slot has someone in it, 0 when it's empty or out of range.
     */
    int field_mod_party_member(int slot, int* hp, int* maximum_hp);

    /**
     * @brief Restore a party member's hit points, up to their maximum, through
     *        the game's own field_heal_status. It doesn't bring back someone at
     *        0. FIELD has to be running.
     * @param slot The party slot, 0 to 2.
     * @param amount How many; 0 or less does nothing.
     * @return 1 when it healed them, 0 when the slot is empty, they're down or
     *         the amount wasn't positive.
     */
    int field_mod_heal(int slot, int amount);

#ifdef __cplusplus
}
#endif
#endif
