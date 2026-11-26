#ifndef LOM_NATIVE_MOD_SEED_H
#define LOM_NATIVE_MOD_SEED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Forget the seed and whether a mod asks for one, for when the mods stop. */
    void lom_native_mod_seed_reset(void);

    /**
     * @brief Note whether a loaded mod asks for a seed ("seed": true).
     * @param wanted 1 when one does.
     */
    void lom_native_mod_seed_set_wanted(int wanted);

    /**
     * @brief Check whether any loaded mod asks for a seed.
     * @return 1 when one does.
     */
    int lom_native_mod_seed_wanted(void);

    /**
     * @brief Note the seed of the game that just started.
     * @param seed The seed.
     */
    void lom_native_mod_seed_set_current(uint32_t seed);

    /**
     * @brief Read the seed of the game being played.
     * @param seed Set to it.
     * @return 1 when a game has started since the mods did, otherwise 0.
     */
    int lom_native_mod_seed_current(uint32_t* seed);

#ifdef __cplusplus
}
#endif
#endif
