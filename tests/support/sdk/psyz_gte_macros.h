#ifndef PSYZ_GTE_MACROS_H
#define PSYZ_GTE_MACROS_H
#include <stdint.h>
unsigned int Psyz_GteDataRead(unsigned reg);

void Psyz_GteDataWrite(unsigned reg, unsigned int value);

void Psyz_GteCommand(unsigned int command);

/**
 * @brief Psy-Q's gte_ldlvl: load a long vector into the GTE's IR1 to IR3.
 * @param source The vector.
 */
static inline void gte_ldlvl(const void* source)
{
    uint32_t words[3];
    __builtin_memcpy(words, source, sizeof(words));

    for (unsigned i = 0; i < 3; ++i)
    {
        Psyz_GteDataWrite(9 + i, words[i]);
    }
}

/**
 * @brief Psy-Q's gte_sqr0: square IR1 to IR3.
 */
static inline void gte_sqr0(void)
{
    Psyz_GteCommand(0x0A00428);
}

/**
 * @brief Psy-Q's gte_sqr12: square IR1 to IR3, shifted down 12 bits.
 */
static inline void gte_sqr12(void)
{
    Psyz_GteCommand(0x0A80428);
}

/**
 * @brief Psy-Q's gte_stlvnl: store MAC1 to MAC3 as a long vector.
 * @param destination Where to.
 */
static inline void gte_stlvnl(void* destination)
{
    uint32_t words[3];

    for (unsigned i = 0; i < 3; ++i)
    {
        words[i] = Psyz_GteDataRead(25 + i);
    }

    __builtin_memcpy(destination, words, sizeof(words));
}

#define gte_SetRotMatrix SetRotMatrix

/**
 * @brief Psy-Q's gte_ldv0: load a short vector into V0.
 * @param source The vector.
 */
static inline void gte_ldv0(const void* source)
{
    uint32_t xy;
    uint16_t z;
    __builtin_memcpy(&xy, source, 4);
    __builtin_memcpy(&z, (const unsigned char*)source + 4, 2);
    Psyz_GteDataWrite(0, xy);
    Psyz_GteDataWrite(1, z);
}

/**
 * @brief Psy-Q's gte_rtv0: rotate V0 by the rotation matrix.
 */
static inline void gte_rtv0(void)
{
    Psyz_GteCommand(0x0486012);
}

/**
 * @brief Psy-Q's gte_stsv: store IR1 to IR3 as a short vector.
 * @param destination Where to.
 */
static inline void gte_stsv(void* destination)
{
    uint16_t components[3];

    for (unsigned i = 0; i < 3; ++i)
    {
        components[i] = Psyz_GteDataRead(9 + i);
    }

    __builtin_memcpy(destination, components, sizeof(components));
}
#endif
