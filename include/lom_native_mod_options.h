#ifndef LOM_NATIVE_MOD_OPTIONS_H
#define LOM_NATIVE_MOD_OPTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Forget the options, for when the mods stop. The player's
     *        preferred choices stay.
     */
    void lom_native_mod_options_reset(void);

    /**
     * @brief Add a loaded mod's option, then its choices with
     *        lom_native_mod_options_add_choice.
     * @param mod The mod's id.
     * @param id The option's id.
     * @param name Its name, for F1.
     * @param description What it does, or empty.
     * @param default_choice The choice it starts on, unless the player prefers another.
     * @return Its index, or -1 when there are too many.
     */
    int lom_native_mod_options_add(const char* mod, const char* id, const char* name, const char* description, int default_choice);

    /**
     * @brief Add a choice to an option; it's put on this one when the player prefers it.
     * @param index The option.
     * @param id The choice's id.
     * @param name Its name, for F1.
     * @param description What it does, or empty.
     */
    void lom_native_mod_options_add_choice(int index, const char* id, const char* name, const char* description);

    /**
     * @brief Have a function called whenever an option's choice changes, to set
     *        the switches it brings.
     * @param listener The function, or NULL for none.
     */
    void lom_native_mod_options_set_listener(void (*listener)(void));

    /**
     * @brief Count the options of the mods that loaded, in load order and each
     *        mod's own order. F1's Mods page shows one row for each.
     * @return How many.
     */
    int lom_native_mod_options_count(void);

    /**
     * @brief The id of the mod an option belongs to.
     * @param index The option, from 0.
     * @return Its mod's id, or NULL when the index is out of range.
     */
    const char* lom_native_mod_options_mod(int index);

    /**
     * @brief An option's id, as mod.json gives it.
     * @param index The option, from 0.
     * @return Its id, or NULL when the index is out of range.
     */
    const char* lom_native_mod_options_id(int index);

    /**
     * @brief An option's name, for F1.
     * @param index The option, from 0.
     * @return Its name, or NULL when the index is out of range.
     */
    const char* lom_native_mod_options_name(int index);

    /**
     * @brief What an option does, for F1's help line.
     * @param index The option, from 0.
     * @return Its description, empty when mod.json has none, or NULL when the
     *         index is out of range.
     */
    const char* lom_native_mod_options_description(int index);

    /**
     * @brief Count an option's choices.
     * @param index The option, from 0.
     * @return How many, or 0 when the index is out of range.
     */
    int lom_native_mod_options_choice_count(int index);

    /**
     * @brief A choice's id, as mod.json gives it.
     * @param index The option, from 0.
     * @param choice The choice, from 0.
     * @return Its id, or NULL when either is out of range.
     */
    const char* lom_native_mod_options_choice_id(int index, int choice);

    /**
     * @brief A choice's name, for F1.
     * @param index The option, from 0.
     * @param choice The choice, from 0.
     * @return Its name, or NULL when either is out of range.
     */
    const char* lom_native_mod_options_choice_name(int index, int choice);

    /**
     * @brief What a choice does, for F1's help line.
     * @param index The option, from 0.
     * @param choice The choice, from 0.
     * @return Its description, empty when mod.json has none, or NULL when
     *         either is out of range.
     */
    const char* lom_native_mod_options_choice_description(int index, int choice);

    /**
     * @brief Which choice an option is on.
     * @param index The option, from 0.
     * @return The choice, from 0, or -1 when the index is out of range.
     */
    int lom_native_mod_options_chosen(int index);

    /**
     * @brief Put an option on a choice, and set the switches it brings at once
     *        through the listener.
     * @param index The option, from 0; one out of range is ignored.
     * @param choice The choice, from 0; one out of range is ignored.
     */
    void lom_native_mod_options_choose(int index, int choice);

    /**
     * @brief Remember the choice the player made for an option, from their
     *        preferences. It's used when the mod loads, or at once if it has.
     *        One that doesn't name a choice the mod has leaves its default.
     * @param mod The mod's id.
     * @param option The option's id.
     * @param choice The choice's id.
     */
    void lom_native_mod_options_prefer(const char* mod, const char* option, const char* choice);

#ifdef __cplusplus
}
#endif
#endif
