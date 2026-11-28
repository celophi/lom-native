#include <psyz/spu.h>

#include <stdio.h>

static const unsigned char sample[16] = {
    0x00, 0x07, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};

/**
 * @brief Run the SPU for a number of frames, throwing the sound away.
 * @param frames How many.
 */
static void advance(int frames)
{
    short output[256 * 2];

    while (frames > 0)
    {
        int count = frames > 256 ? 256 : frames;
        Psyz_SpuPullSamples(output, count);
        frames -= count;
    }
}

/**
 * @brief Reset the SPU and key voice 0 on with an envelope.
 * @param low The envelope's low register.
 * @param high The envelope's high register.
 */
static void start(unsigned short low, unsigned short high)
{
    Psyz_SpuReset(0);
    Psyz_SpuMemWrite(0x2000, sample, sizeof(sample));
    Psyz_SpuWrite(0x04, 0x1000);
    Psyz_SpuWrite(0x06, 0x400);
    Psyz_SpuWrite(0x08, low);
    Psyz_SpuWrite(0x0A, high);
    Psyz_SpuWrite(0x0E, 0x400);
    Psyz_SpuWrite(0x1AA, 0xC000);
    Psyz_SpuWrite(0x188, 1);
    advance(256);
}

int main(void)
{
    int failed = 0;
    Psyz_SpuInit();

    const unsigned short fades[] = {0x4C87, 0x4D07, 0x4E47, 0x4E87};

    for (unsigned i = 0; i < sizeof(fades) / sizeof(fades[0]); i++)
    {
        start(0x000F, 0x5FC7);
        unsigned before = Psyz_SpuRead(0x0C);
        Psyz_SpuWrite(0x0A, fades[i]);
        advance(18000);
        unsigned after = Psyz_SpuRead(0x0C);

        if (before < 0x3000 || (i < 2 ? after != 0 : after >= before - 5000))
        {
            fprintf(stderr, "live sustain %04X: ENVX %04X -> %04X (expected decay)\n", fades[i], before, after);
            failed = 1;
        }
    }

    start(0x000F, 0x1FFF);
    Psyz_SpuWrite(0x18C, 1);
    advance(256);
    unsigned before = Psyz_SpuRead(0x0C);
    Psyz_SpuWrite(0x0A, 0x1FC0);
    advance(4);

    if (before < 0x3000 || Psyz_SpuRead(0x0C) != 0)
    {
        fprintf(stderr, "live release: ENVX %04X -> %04X (expected zero)\n", before, Psyz_SpuRead(0x0C));
        failed = 1;
    }

    start(0x7F0F, 0x1FC0);
    before = Psyz_SpuRead(0x0C);
    Psyz_SpuWrite(0x08, 0x000F);
    advance(4);

    if (before != 0 || Psyz_SpuRead(0x0C) < 0x3000)
    {
        fprintf(stderr, "live attack: ENVX %04X -> %04X (expected attack progress)\n", before, Psyz_SpuRead(0x0C));
        failed = 1;
    }

    if (!failed)
    {
        puts("LoM SPU live ADSR smoke passed (sustain, release, attack).");
    }

    return failed;
}
