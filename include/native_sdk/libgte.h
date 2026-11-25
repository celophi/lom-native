#ifndef LOM_NATIVE_SDK_LIBGTE_H
#define LOM_NATIVE_SDK_LIBGTE_H

#include_next <libgte.h>
#include <psyz/gte.h>

#ifndef ONE
#define ONE 4096
#endif

#undef gte_ldv0
#define gte_ldv0(x) Psyz_GteLdv0((SVECTOR*)(x))
#undef gte_ldv3
#define gte_ldv3(x, y, z) Psyz_GteLdv3((SVECTOR*)(x), (SVECTOR*)(y), (SVECTOR*)(z))
#undef gte_ldv01c
#define gte_ldv01c(x) Psyz_GteLdv01c((SVECTOR*)(x))
#undef gte_ldv3c
#define gte_ldv3c(x) Psyz_GteLdv3c((SVECTOR*)(x))

/**
 * @brief Read a GTE data register (psyz/gte.h, declared here too since it can
 *        include this before its own declaration).
 * @param reg The register, 0 to 31.
 * @return Its value.
 */
unsigned int Psyz_GteDataRead(unsigned reg);

/**
 * @brief Write a GTE data register (psyz/gte.h, declared here too since it can
 *        include this before its own declaration).
 * @param reg The register, 0 to 31.
 * @param value Its value.
 */
void Psyz_GteDataWrite(unsigned reg, unsigned int value);

/**
 * @brief Load IR1 to IR3 from a long vector, as Psy-Q's gte_ldlvl did.
 * @param vector Three 32-bit words.
 */
static inline void lom_native_gte_ldlvl(const void* vector)
{
    const int* words = (const int*)vector;
    Psyz_GteDataWrite(9, (unsigned int)words[0]);
    Psyz_GteDataWrite(10, (unsigned int)words[1]);
    Psyz_GteDataWrite(11, (unsigned int)words[2]);
}

#define gte_ldlvl(x) lom_native_gte_ldlvl(x)

/**
 * @brief Psy-Q's VectorNormal, which the game calls by that name: normalize a
 *        vector with the GTE the way the original does. PSY-Z's doesn't return
 *        anything, and the game uses the result.
 * @param input The vector.
 * @param output Its normal.
 * @return The vector's squared length.
 */
long lom_native_vector_normal(VECTOR* input, VECTOR* output);
#define VectorNormal lom_native_vector_normal

#endif
