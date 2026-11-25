#ifndef LOM_NATIVE_GP0_HASH_H
#define LOM_NATIVE_GP0_HASH_H

#include <stdint.h>

/**
 * @brief Start a new fingerprint of the GPU commands.
 */
void lom_native_gp0_hash_reset(void);

/**
 * @brief The fingerprint of the GPU commands since the reset: an FNV-1a hash
 *        of the bits of each word the GPU reads, without image data.
 * @return The hash.
 */
uint32_t lom_native_gp0_hash(void);

/**
 * @brief Count the GPU commands since the reset, not counting NOPs.
 * @return The count.
 */
unsigned lom_native_gp0_commands(void);

#endif
