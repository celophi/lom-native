#include <psyz.h>
#include <libgte.h>

#include <stdio.h>
#ifdef LOM_NATIVE_GTE_LEADING_BITS
#undef gte_ldv0
#undef gte_rtv0
#undef gte_stsv
#undef gte_ldlvl
#undef gte_stlvnl
#undef gte_sqr0
#undef gte_sqr12
#undef gte_SetRotMatrix
#include "../support/sdk/psyz_gte_macros.h"
#endif

/**
 * @brief Verify that the native toolchain can compile, link, and execute PSY-Z.
 * @return Zero when the PSY-Z GTE sanity checks pass, otherwise nonzero.
 */
int main(void)
{
    long root;
    int sine;
    int cosine;

    root = SquareRoot0(144);
    sine = rsin(1024);
    cosine = rcos(1024);

    printf("Legend of Mana Native - PSY-Z bootstrap\n");
    printf("SquareRoot0(144) = %ld\n", root);
    printf("rsin(1024)       = %d\n", sine);
    printf("rcos(1024)       = %d\n", cosine);

    if (root != 12 || sine != 4096 || cosine != 0)
    {
        fprintf(stderr, "PSY-Z bootstrap validation failed.\n");
        return 1;
    }

#ifdef LOM_NATIVE_GTE_LEADING_BITS
    for (unsigned exponent = 0; exponent <= 30; exponent += 2)
    {
        long value = 1L << exponent;

        if (SquareRoot0(value) != (1L << (exponent / 2)) || SquareRoot12(value) != (1L << (exponent / 2 + 6)))
        {
            return 1;
        }
    }

    if (SquareRoot0(19949) != 140 || SquareRoot0(0) != 0)
    {
        return 1;
    }

    MATRIX rotation = {.m = {{0, -4096, 0}, {4096, 0, 0}, {0, 0, 4096}}};
    short input[] = {-3, 4, -7, 0}, output[] = {0, 0, 0, 1234};
    SetRotMatrix(&rotation);
    gte_ldv0(input);
    gte_rtv0();
    gte_stsv(output);

    if (output[0] != -4 || output[1] != -3 || output[2] != -7 || output[3] != 1234)
    {
        return 1;
    }

    const unsigned values[] = {0, 0xFFFFFFFFu, 1, 0xFFFFFFFEu, 0x40000000u, 0xBFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 0x0000FFFFu, 0xFFFF0000u};
    const unsigned counts[] = {32, 32, 31, 31, 1, 1, 1, 1, 16, 16};

    for (unsigned i = 0; i < sizeof(values) / sizeof(*values); ++i)
    {
        Psyz_GteDataWrite(30, values[i]);
        Psyz_GteDataWrite(31, 99);

        if (Psyz_GteDataRead(30) != values[i] || Psyz_GteDataRead(31) != counts[i])
        {
            fprintf(stderr, "GTE leading-bit registers failed for %08X\n", values[i]);
            return 1;
        }
    }
#endif
    puts("PSY-Z bootstrap validation passed.");
    return 0;
}
