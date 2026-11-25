#include <psyz/spu.h>

#include <stdio.h>
#include <string.h>

#define TEST_VOICE 1
#define TEST_VOICE_BASE (TEST_VOICE * 0x10)
#define TEST_SAMPLE_ADDR 0x2000
#define TEST_VOICE_BIT (1u << TEST_VOICE)
#define SPU_KEY_ON_LOW 0x188
#define SPU_ENDX_LOW 0x19C
#define SPU_CONTROL 0x1AA

static const unsigned char s_one_shot_adpcm[16] = {
    0x00, 0x01, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};

int main(void)
{
    short output[256 * 2];
    unsigned short envx;
    unsigned short endx;

    Psyz_SpuInit();
    Psyz_SpuReset(0);
    Psyz_SpuMemWrite(TEST_SAMPLE_ADDR, s_one_shot_adpcm, sizeof(s_one_shot_adpcm));

    Psyz_SpuWrite(TEST_VOICE_BASE + 0x00, 0x3FFF);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x02, 0x3FFF);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x04, 0x1000);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x06, TEST_SAMPLE_ADDR >> 3);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x08, 0x000F);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x0A, 0x1FC0);
    Psyz_SpuWrite(TEST_VOICE_BASE + 0x0E, TEST_SAMPLE_ADDR >> 3);
    Psyz_SpuWrite(SPU_CONTROL, 0xC000);

    Psyz_SpuWrite(SPU_KEY_ON_LOW, TEST_VOICE_BIT);
    Psyz_SpuPullSamples(output, 256);

    envx = Psyz_SpuRead(TEST_VOICE_BASE + 0x0C);
    endx = Psyz_SpuRead(SPU_ENDX_LOW);

    if (envx != 0)
    {
        fprintf(stderr, "one-shot voice left stale ENVX 0x%04X\n", envx);
        return 1;
    }

    if ((endx & TEST_VOICE_BIT) == 0)
    {
        fprintf(stderr, "one-shot voice did not set ENDX (0x%04X)\n", endx);
        return 1;
    }

    Psyz_SpuWrite(SPU_KEY_ON_LOW, TEST_VOICE_BIT);
    endx = Psyz_SpuRead(SPU_ENDX_LOW);

    if (endx & TEST_VOICE_BIT)
    {
        fprintf(stderr, "key-on did not clear ENDX (0x%04X)\n", endx);
        return 1;
    }

    printf("LoM SPU one-shot voice completion smoke passed.\n");
    printf("  ENVX after end: 0x%04X\n", envx);
    printf("  ENDX after end: 0x%04X\n", (unsigned short)(TEST_VOICE_BIT));
    return 0;
}
