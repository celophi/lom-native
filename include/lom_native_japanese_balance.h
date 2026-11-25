#ifndef LOM_NATIVE_JAPANESE_BALANCE_H
#define LOM_NATIVE_JAPANESE_BALANCE_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Whether the US game plays by the Japanese version's combat and
     *        item balance, the japanese_balance switch.
     * @return 1 when it does.
     */
    int lom_japanese_balance(void);

    /**
     * @brief Put the Japanese version's numbers in a scene file the game has
     *        just read, when the switch is on: the stats, growth and drops of
     *        its monsters, the items they leave, guests, pet equipment and
     *        forging materials. A scene whose records aren't the US disc's is
     *        left as it is.
     * @param scene The scene's number.
     * @param scene_file The scene file.
     * @param size How many bytes of it were read.
     */
    void lom_japanese_balance_scene(int scene, unsigned char* scene_file, unsigned int size);

    /**
     * @brief The Japanese version's monster level for a level rank.
     * @param rank The rank, 0 to 63; one past the end gives the last.
     * @return The level.
     */
    int lom_japanese_balance_level(int rank);

    /**
     * @brief The Japanese version's power for a weapon technique.
     * @param index The technique's place in the action bank's weapon technique
     *        table.
     * @param power Its US power, returned for an index past the table.
     * @return The power.
     */
    int lom_japanese_balance_technique_power(int index, int power);

#ifdef __cplusplus
}
#endif

#endif
