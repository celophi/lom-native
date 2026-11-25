#ifndef LOM_NATIVE_RANDOM_H
#define LOM_NATIVE_RANDOM_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief The game's random numbers, the same sequence as the PS1's rand.
     * @return 0 to 32767.
     */
    int lom_native_rand(void);

    /**
     * @brief Seed the game's random numbers, as the PS1's srand did.
     * @param seed The seed.
     */
    void lom_native_srand(unsigned int seed);

#ifdef __cplusplus
}
#endif
#endif
