#include <stdio.h>

#include "main/card_callbacks.h"

/**
 * @brief Verify that an unchanged resident translation unit is compiled directly from lom-decomp.
 * @return Zero when the call into the decomp's code finishes.
 */
int main(void)
{
    card_prepare_pet_transfer(NULL, NULL);
    puts("lom-decomp resident source smoke passed.");
    return 0;
}
