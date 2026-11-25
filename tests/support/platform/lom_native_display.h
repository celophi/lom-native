#ifndef LOM_NATIVE_DISPLAY_H
#define LOM_NATIVE_DISPLAY_H

#include <psyz.h>
#include <libgpu.h>

/**
 * @brief Initialize the LoM-native display state using the original PS1 layout.
 */
void lom_native_display_init(void);

/**
 * @brief Render and present one LoM-derived bootstrap frame.
 */
void lom_native_display_present(void);

#endif
