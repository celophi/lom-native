#ifndef LOM_NATIVE_PSYZ_SDK_H
#define LOM_NATIVE_PSYZ_SDK_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Set a root counter's target and mode, and make it the one the
     *        next InterruptCallback goes on.
     * @param spec The counter, as the SDK's RCntCNT constants name it.
     * @param target The count it runs to; 0 means 65536.
     * @param mode The SDK's RCntMd flags.
     * @return 1, or 0 for a counter that doesn't work.
     */
    long SetRCnt(long spec, unsigned short target, long mode);

    /**
     * @brief Start root counter 2's interrupts.
     * @param spec The counter.
     * @return 1, or 0 for any counter but 2.
     */
    long StartRCnt(long spec);

    /**
     * @brief Stop root counter 2's interrupts.
     * @param spec The counter.
     * @return 1, or 0 for any counter but 2.
     */
    long StopRCnt(long spec);

    /**
     * @brief Set the function the root counter SetRCnt last set runs, the way
     *        the retail SDK does.
     * @param irq Only checked to be under 0x100.
     * @param callback The function, or NULL to remove it.
     * @return The previous function.
     */
    void* InterruptCallback(int irq, void (*callback)());

    /**
     * @brief Keep PSY-Z's interrupts out until PS1_ExitCriticalSection.
     * @return 1 when this call entered the critical section, 0 when it was
     *         already in one, as Psy-Q's EnterCriticalSection does.
     */
    int PS1_EnterCriticalSection(void);

    /**
     * @brief Let PSY-Z's interrupts back in.
     */
    void PS1_ExitCriticalSection(void);

    /**
     * @brief Stop the next card read or write from reporting a new card
     *        (EvSpNEW), as the SDK's does. Does nothing in PSY-Z.
     */
    void _new_card(void);

    extern void (*volatile _spu_transferCallback)();

#ifdef __cplusplus
}
#endif

#endif
