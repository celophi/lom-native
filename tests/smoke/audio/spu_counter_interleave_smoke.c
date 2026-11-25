#include <psyz/spu.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_VOICE_BASE 0x00
#define TEST_SAMPLE_ADDR 0x2000
#define SPU_MAIN_VOL_LEFT 0x180
#define SPU_MAIN_VOL_RIGHT 0x182
#define SPU_KEY_ON_LOW 0x188
#define SPU_KEY_OFF_LOW 0x18C
#define SPU_CONTROL 0x1AA
#define TEST_FRAMES 512

static volatile unsigned s_counter_ticks;

extern long SetRCnt(long spec, unsigned short target, long mode);

extern long StartRCnt(long spec);

extern void* InterruptCallback(int source, void (*callback)());

static const unsigned char s_looping_adpcm[16] = {
    0x00, 0x07, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};

/**
 * @brief A root counter callback that keys voice 0 on at the first tick and off
 *        at the second, as the sound driver would.
 */
static void sequence_tick(void)
{
    s_counter_ticks++;

    if (s_counter_ticks == 1)
    {
        Psyz_SpuWrite(SPU_KEY_ON_LOW, 1u);
    }
    else if (s_counter_ticks == 2)
    {
        Psyz_SpuWrite(SPU_KEY_OFF_LOW, 1u);
    }
}

/**
 * @brief Add up how loud a render is.
 * @param samples The render.
 * @return The sum of its samples' sizes.
 */
static long long buffer_energy(const short* samples)
{
    long long energy;
    int i;

    energy = 0;

    for (i = 0; i < TEST_FRAMES * 2; i++)
    {
        energy += llabs((long long)samples[i]);
    }

    return energy;
}

/**
 * @brief Verify that root-counter callbacks are interleaved with SPU samples.
 * @return Zero when a one-tick note produces audible samples inside one pull.
 */
int main(void)
{
    short output[TEST_FRAMES * 2];
    long long energy;

    Psyz_SpuInit();
    Psyz_SpuReset(0);
    Psyz_SpuMemWrite(TEST_SAMPLE_ADDR, s_looping_adpcm, sizeof(s_looping_adpcm));

    Psyz_SpuWrite(TEST_VOICE_BASE + 0x00, 0x3FFF);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x02, 0x3FFF);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x04, 0x1000);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x06, TEST_SAMPLE_ADDR >> 3);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x08, 0x000F);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x0A, 0x0000);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x0E, TEST_SAMPLE_ADDR >> 3);
    Psyz_SpuWrite(SPU_MAIN_VOL_LEFT, 0x3FFF);
    Psyz_SpuWrite(SPU_MAIN_VOL_RIGHT, 0x3FFF);
    Psyz_SpuWrite(SPU_CONTROL, 0xC000);

    if (SetRCnt(0xF2000002, 0x44E8, 0x1000) == 0)
    {
        fprintf(stderr, "Failed to configure root counter 2.\n");
        return 1;
    }

    InterruptCallback(0, sequence_tick);

    if (StartRCnt(0xF2000002) == 0)
    {
        fprintf(stderr, "Failed to enable root counter 2 interrupts.\n");
        return 1;
    }

    memset(output, 0, sizeof(output));
    Psyz_SpuPullSamples(output, TEST_FRAMES);
    energy = buffer_energy(output);

    if (s_counter_ticks < 2)
    {
        fprintf(stderr, "Expected at least two sequencer ticks, got %u.\n", s_counter_ticks);
        return 2;
    }

    if (energy == 0)
    {
        fprintf(stderr, "Counter callbacks were not interleaved with SPU rendering; one-tick note was silent.\n");
        return 3;
    }

    printf("LoM SPU counter interleave smoke passed.\n");
    printf("  root-counter ticks: %u\n", s_counter_ticks);
    printf("  one-tick energy:    %lld\n", energy);
    return 0;
}
