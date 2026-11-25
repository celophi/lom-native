#ifndef LOM_NATIVE_SPU_SHADOW_H
#define LOM_NATIVE_SPU_SHADOW_H

#include <stdint.h>

#define LOM_NATIVE_SPU_REGISTER_BYTES 0x200u

/** @brief Fill the copy at @p registers from PSY-Z's SPU. */
void lom_native_spu_shadow_begin(volatile uint16_t* registers);

/** @brief Write the registers AKAO changed since the last begin back to PSY-Z's SPU. */
void lom_native_spu_shadow_commit(volatile uint16_t* registers);

#endif
