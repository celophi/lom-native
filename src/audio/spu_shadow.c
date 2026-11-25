#include "lom_native_spu_shadow.h"

#define u_long LomPsyzULong
#include <psyz/spu.h>
#undef u_long

static uint16_t s_before[LOM_NATIVE_SPU_REGISTER_BYTES / 2];

void lom_native_spu_shadow_begin(volatile uint16_t* registers)
{
    for (unsigned i = 0; i < LOM_NATIVE_SPU_REGISTER_BYTES / 2; i++)
    {
        s_before[i] = Psyz_SpuRead(i * 2);
        registers[i] = s_before[i];
    }
}

/**
 * @brief Check whether an SPU register is one of the key on and key off
 *        registers, which take effect when written rather than holding a value.
 * @param offset The register's offset.
 * @return 1 when it is.
 */
static int is_key_register(unsigned offset)
{
    return offset == 0x188u || offset == 0x18Au || offset == 0x18Cu || offset == 0x18Eu;
}

/**
 * @brief Write a key on or key off register the driver changed, then clear it,
 *        so the keys it set act once.
 * @param registers The shadow registers.
 * @param offset The register's offset.
 */
static void commit_key_register(volatile uint16_t* registers, unsigned offset)
{
    unsigned i = offset / 2;
    uint16_t value = registers[i];

    if (value == s_before[i])
    {
        return;
    }

    Psyz_SpuWrite(offset, value);
    Psyz_SpuWrite(offset, 0);
    registers[i] = 0;
}

void lom_native_spu_shadow_commit(volatile uint16_t* registers)
{
    for (unsigned i = 0; i < LOM_NATIVE_SPU_REGISTER_BYTES / 2; i++)
    {
        unsigned offset = i * 2;

        if (is_key_register(offset))
        {
            continue;
        }

        if (registers[i] != s_before[i])
        {
            Psyz_SpuWrite(offset, registers[i]);
        }
    }

    commit_key_register(registers, 0x18Cu);
    commit_key_register(registers, 0x18Eu);
    commit_key_register(registers, 0x188u);
    commit_key_register(registers, 0x18Au);
}
