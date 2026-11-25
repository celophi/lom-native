#include <libgte.h>
#include "../../support/sdk/lom_native_gte_contract.h"
#include <stdio.h>
#include "../../support/lom_check.h"

int main(void)
{
    volatile int axes[][3] = {{0, 1, 0}, {-1, 1, -512}, {-1, 0, -1024}, {-1, -1, -1536}, {0, -1, 2048}, {1, -1, 1536}, {1, 0, 1024}, {1, 1, 512}, {0, 0, 0}};

    for (unsigned i = 0; i < sizeof(axes) / sizeof(*axes); ++i)
    {
        CHECK(ratan2(axes[i][0], axes[i][1]) == axes[i][2]);
    }

    CHECK(SquareRoot0(144) == 12 && rsin(-1024) == -4096 && rcos(-1024) == 0);
    InitGeom();
    CHECK(NormalClip(0, 10, 10L << 16) == 100);
    CHECK(NormalClip(0, 10L << 16, 10) == -100);
    MATRIX matrix = {.m = {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}};
    CHECK(RotMatrixZ(-1024, &matrix) == &matrix);
    CHECK(matrix.m[0][0] == 0 && matrix.m[0][1] == 4096 && matrix.m[1][0] == -4096);
    puts("Shared SDK/PSY-Z ABI: layouts, 19 signatures, signed octants, rotation and clipping passed.");
    return 0;
}
