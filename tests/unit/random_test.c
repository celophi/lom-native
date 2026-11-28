#include "lom_native_random.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const int expected[] = {16838, 5758, 10113, 17515, 31051, 5627, 23010, 7419};
    lom_native_srand(1);

    for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        int result = lom_native_rand();

        if (result != expected[i])
        {
            fprintf(stderr, "Psy-Q RNG index %u: %d expected %d\n", i, result, expected[i]);
            return 1;
        }
    }

    lom_native_srand(1);
    srand(923);
    (void)rand();

    if (lom_native_rand() != expected[0])
    {
        return 2;
    }

    for (unsigned seed = 0; seed < 3; seed++)
    {
        lom_native_srand(seed == 2 ? 0xFFFFFFFFu : seed);

        for (unsigned i = 0; i < 100000; i++)
        {
            int result = lom_native_rand();

            if (result < 0 || result > 32767 || ((result * 100) >> 15) >= 100)
            {
                return 3;
            }
        }
    }

    puts("Psy-Q random sequence, range, and host isolation passed.");
    return 0;
}
