#ifndef LOM_NATIVE_GPU_H
#define LOM_NATIVE_GPU_H

/**
 * @brief Start PSY-Z's platform and GPU, clear VRAM and show a 320x240 screen.
 * If the backend doesn't start, it says why in a dialog unless
 * LOM_NO_ERROR_DIALOG is set.
 * @return 0, or -1 if PSY-Z's backend didn't start.
 */
int lom_native_gpu_init(void);

/** @brief Stop PSY-Z's platform. AKAO's audio stream has to be gone first. */
void lom_native_gpu_shutdown(void);

/**
 * @brief Show the frame just drawn. PSY-Z only presents from a mode-0 VSync,
 * which a session replaying the old timing gets as a real VSync(0).
 */
void lom_native_gpu_present(void);

/** @brief Whether the window was closed or the player asked to quit. */
int lom_native_gpu_quit_requested(void);

#endif
