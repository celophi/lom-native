#ifndef LOM_NATIVE_SESSION_H
#define LOM_NATIVE_SESSION_H

/**
 * @brief Start recording the session for a bug report, when LOM_RECORD=1:
 *        controller input and timing, a copy of the memory cards, a manifest
 *        and everything the game prints, in a new folder under the sessions
 *        folder (LOM_SESSION_DIR, or the user's state folder). Old sessions are
 *        pruned to keep the total small.
 * @param disc_root The extracted disc, for the manifest.
 * @return 0 when recording or when it's off, -1 when the folder can't be set
 *         up; the game carries on either way.
 */
int lom_native_session_record_start(const char* disc_root);

/**
 * @brief Start replaying a session's inputs.bin, input by input. Versions 1
 *        and 2 need the virtual clock.
 * @param inputs_path The recording's inputs.bin.
 * @return 0, or -1 when it can't be read or isn't a version this build reads.
 */
int lom_native_session_replay_start(const char* inputs_path);

/**
 * @brief Note why FIELD stopped, which the recording keeps as how the run
 *        ended. Only the first reason counts.
 * @param reason FIELD's reason.
 */
void lom_native_session_note_stop(const char* reason);

/**
 * @brief Find the folder this run is recording into.
 * @return The folder, or NULL when it isn't recording.
 */
const char* lom_native_session_folder(void);

/**
 * @brief Finish the recording: write what's buffered and how the run ended,
 *        unless FIELD noted a reason first. Does nothing when it isn't
 *        recording or has already finished.
 * @param end_reason How it ended: "quit", "exit" and the like.
 */
void lom_native_session_finish(const char* end_reason);

/**
 * @brief Save what's buffered and the reason when the game is crashing. Safe
 *        in a signal handler: it only writes to files already open.
 * @param reason The crash, like "signal 11".
 */
void lom_native_session_crashed(const char* reason);

/**
 * @brief Check how a replay is going.
 * @return 0 while it's running, 1 once every input has played back, 2 when
 *         the game went somewhere the recording didn't.
 */
int lom_native_session_replay_state(void);

/**
 * @brief Set a function that writes the controller bytes each time the game
 *        samples the pads, which the route tests use to play the game. A
 *        replay ignores it.
 * @param script Takes the vertical blank count and both ports' receive
 *        buffers. NULL for none.
 */
void lom_native_session_set_pad_script(void (*script)(unsigned long long vblank, unsigned char* port_buffers[2]));

/**
 * @brief The game has sampled the pads: run the pad script, record the
 *        buffers, or replace them with the recording's.
 * @param port_buffers Both ports' receive buffers.
 */
void lom_native_session_pad_sample(unsigned char* port_buffers[2]);

#endif
