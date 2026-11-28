#include <psyz/spu.h>
#include "lom_native_akao.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif

#define SPU_MMIO_PAGE 0x1F801000u
#define SPU_MMIO_SIZE 0x1000u
#define SPU_REG_BASE 0x1F801C00u
#define TEST_SAMPLE_ADDR 0x2000u
#define TEST_FRAMES 256

#define SPU_MAIN_VOL_LEFT 0x180u
#define SPU_MAIN_VOL_RIGHT 0x182u
#define SPU_KEY_ON_LOW 0x188u
#define SPU_KEY_OFF_LOW 0x18Cu
#define SPU_CONTROL 0x1AAu

static const unsigned char s_looping_adpcm[16] = {
    0x00, 0x07, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};

extern unsigned char lom_native_spu_registers[];
#define SPU_REGISTERS ((volatile uint16_t*)lom_native_spu_registers)

/**
 * @brief Add up how loud a render is.
 * @param samples The render.
 * @return The sum of its samples' sizes.
 */
static long long buffer_energy(const short* samples)
{
    long long energy = 0;
    int i;

    for (i = 0; i < TEST_FRAMES * 2; i++)
    {
        energy += llabs((long long)samples[i]);
    }

    return energy;
}

/**
 * @brief Verify that the native MMIO bridge preserves AKAO KOFF-before-KON
 *        retriggers.
 * @return Zero when a same-transaction voice retrigger remains audible.
 */
int main(void)
{
    volatile uint16_t* mmio;
    short output[TEST_FRAMES * 2];
    long long energy;

    Psyz_SpuInit();
    Psyz_SpuReset(0);
    Psyz_SpuMemWrite(TEST_SAMPLE_ADDR, s_looping_adpcm, sizeof(s_looping_adpcm));

    Psyz_SpuWrite(0x00u, 0x3FFFu);
    Psyz_SpuWrite(0x02u, 0x3FFFu);
    Psyz_SpuWrite(0x04u, 0x1000u);
    Psyz_SpuWrite(0x06u, TEST_SAMPLE_ADDR >> 3);
    Psyz_SpuWrite(0x08u, 0x000Fu);
    Psyz_SpuWrite(0x0Au, 0x0000u);
    Psyz_SpuWrite(0x0Eu, TEST_SAMPLE_ADDR >> 3);
    Psyz_SpuWrite(SPU_MAIN_VOL_LEFT, 0x3FFFu);
    Psyz_SpuWrite(SPU_MAIN_VOL_RIGHT, 0x3FFFu);
    Psyz_SpuWrite(SPU_CONTROL, 0xC000u);

    lom_native_akao_disable_event(0);
    mmio = SPU_REGISTERS;
    mmio[SPU_KEY_OFF_LOW / 2] = 1u;
    mmio[SPU_KEY_ON_LOW / 2] = 1u;
    lom_native_akao_enable_event(0);

    memset(output, 0, sizeof(output));
    Psyz_SpuPullSamples(output, TEST_FRAMES);
    energy = buffer_energy(output);

    if (energy == 0)
    {
        fprintf(stderr, "same-tick KOFF/KON retrigger was silent\n");
        return 2;
    }

    printf("LoM AKAO MMIO retrigger smoke passed.\n");
    printf("  retrigger energy: %lld\n", energy);
    return 0;
}
