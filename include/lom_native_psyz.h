#ifndef LOM_NATIVE_PSYZ_H
#define LOM_NATIVE_PSYZ_H
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Let the game handle a quit request instead of PSY-Z exiting.
     * @param hook Called once PSY-Z's audio and platform have shut down. NULL
     *        goes back to PSY-Z exiting the process.
     */
    void lom_native_psyz_set_quit_hook(void (*hook)(void));

    /**
     * @brief Say whether a port GPU command is laid out like a primitive.
     * @param opcode The command's first byte, 0 to 0xFF.
     * @param packet Nonzero if its packet is in 32-bit words like a
     *        primitive's, so the queue splits it into 64-bit words the same way.
     * @return 0, or -1 for an opcode past 0xFF.
     */
    int lom_psyz_gpu_command_is_packet(unsigned int opcode, int packet);

    /**
     * @brief Check whether audio runs on the tests' virtual clock.
     * @return Nonzero when each vertical blank pulls a fixed number of samples
     *        instead of the audio device pulling them.
     */
    int Psyz_AudioIsVirtualClock(void);

    /**
     * @brief Set how many audio frames the next virtual blank pulls.
     * @param frames The recorded count, or -1 to go back to the fixed rate.
     */
    void Psyz_AudioSetVirtualBlankFrames(int frames);

    /**
     * @brief Count the audio frames pulled from the SPU so far.
     * @return Frames since the audio started. Safe to call from any thread.
     */
    unsigned long long Psyz_AudioPulledFrames(void);

    /**
     * @brief Forget the vertical blank being delivered, before a quit or a
     *        failure jumps out of the middle of its callbacks.
     */
    void Psyz_VideoAbortVBlankDelivery(void);

    /**
     * @brief Set a function to run on every vertical blank.
     * @param hook Called once per blank, or NULL for none.
     */
    void Psyz_VideoSetVBlankHook(void (*hook)(void));

    /**
     * @brief Set where a replay gets the extra blanks it recorded.
     * @param hook On the virtual clock, called after each blank until it
     *        returns 0. Each positive return delivers one more blank with no
     *        frame shown, and a negative one fails the clock. NULL for none.
     */
    void Psyz_VideoSetElapsedReplayHook(int (*hook)(void));

    /**
     * @brief Set a function that can replace the horizontal count PSY-Z
     *        reports.
     * @param hook Takes the real count and returns the one to report, or NULL
     *        for none.
     */
    void Psyz_VideoSetHCountHook(unsigned (*hook)(unsigned));

    /**
     * @brief Set a function that's told the blank count each VSync(-1)
     *        returns.
     * @param hook Takes the count. Its return value is ignored, so it can't
     *        change the count. NULL for none.
     */
    void Psyz_VideoSetCountHook(int (*hook)(unsigned));

    /**
     * @brief Check whether the blank being delivered also showed a frame.
     * @return Nonzero if it did.
     */
    int Psyz_VideoVBlankWasPresented(void);

    /**
     * @brief Turn on the VSync timing that version 1 and 2 recordings used.
     * @param enabled Nonzero to turn it on, which needs the virtual clock.
     * @return 0, or -1 if it was asked for without the virtual clock.
     */
    int Psyz_SetLegacyVSyncReplayProfile(int enabled);

    /**
     * @brief Check whether the timing for version 1 and 2 recordings is on.
     * @return Nonzero if it is.
     */
    int Psyz_LegacyVSyncReplayProfileActive(void);

    /**
     * @brief Check whether the player has asked to quit.
     * @return Nonzero once they have.
     */
    int Psyz_QuitRequested(void);

    /**
     * @brief Start PSY-Z's window, renderer and audio.
     * @return Whether it started.
     */
    bool InitPlatform(void);

    /**
     * @brief Shut PSY-Z's platform down, so InitPlatform can start it again.
     */
    void ResetPlatform(void);

#ifdef __cplusplus
}
#endif

#endif
