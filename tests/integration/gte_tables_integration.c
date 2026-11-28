#include "common.h"
#include <libgte.h>

#include "lom_native_cd.h"
#include "lom_native_resident.h"
#include "main/cdrom.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

MATRIX* RotMatrix_gte(SVECTOR* angles, MATRIX* matrix);

static const double k_one = 4096.0;
static const double k_pi = 3.14159265358979323846;

/**
 * @brief Check VectorNormal gives a vector 4096 long, pointing the same way,
 *        and the same length for the opposite vector. It reads the main
 *        executable's square root table, so a wrong table address makes it
 *        come out the wrong length.
 * @param x The vector's X.
 * @param z The vector's Z.
 * @return How many checks failed.
 */
static int check_normal(s32 x, s32 z)
{
    VECTOR input = {x, 0, z, 0};
    VECTOR opposite = {-x, 0, -z, 0};
    VECTOR normal;
    VECTOR opposite_normal;
    int failed = 0;

    VectorNormal(&input, &normal);
    VectorNormal(&opposite, &opposite_normal);

    double length = sqrt((double)normal.vx * normal.vx + (double)normal.vz * normal.vz);
    double opposite_length = sqrt((double)opposite_normal.vx * opposite_normal.vx + (double)opposite_normal.vz * opposite_normal.vz);
    double expected_x = k_one * x / sqrt((double)x * x + (double)z * z);
    double expected_z = k_one * z / sqrt((double)x * x + (double)z * z);

    if (fabs(length - k_one) > 8 || fabs(opposite_length - k_one) > 8)
    {
        printf("VectorNormal(%d, %d): lengths %.0f and %.0f for it and its opposite, not 4096\n", x, z, length, opposite_length);
        failed++;
    }

    if (fabs(normal.vx - expected_x) > 8 || fabs(normal.vz - expected_z) > 8)
    {
        printf("VectorNormal(%d, %d) is (%d, %d), not about (%.0f, %.0f)\n", x, z, normal.vx, normal.vz, expected_x, expected_z);
        failed++;
    }

    return failed;
}

/**
 * @brief Check RotMatrix_gte's rotation about X matches the real sine and
 *        cosine. It reads the main executable's sine table, so a wrong table
 *        address gives the wrong rotation.
 * @param angle The angle, 4096 to a turn.
 * @return How many checks failed.
 */
static int check_rotation(s16 angle)
{
    SVECTOR angles = {angle, 0, 0, 0};
    MATRIX matrix;
    double sine = k_one * sin(angle * 2 * k_pi / 4096);
    double cosine = k_one * cos(angle * 2 * k_pi / 4096);

    RotMatrix_gte(&angles, &matrix);

    if (matrix.m[0][0] != 4096 || fabs(matrix.m[1][1] - cosine) > 2 || fabs(matrix.m[2][2] - cosine) > 2 ||
        fabs(fabs((double)matrix.m[1][2]) - fabs(sine)) > 2 || fabs(fabs((double)matrix.m[2][1]) - fabs(sine)) > 2 ||
        matrix.m[1][2] != -matrix.m[2][1])
    {
        printf("RotMatrix_gte about X by %d: [%d %d / %d %d], not cos %.0f and sin %.0f\n", angle, matrix.m[1][1], matrix.m[1][2], matrix.m[2][1],
               matrix.m[2][2], cosine, sine);
        return 1;
    }

    return 0;
}

int main(int argc, char** argv)
{
    static const s32 lengths[] = {1, 7, 8, 14, 16, 100, 4096, 70000};
    int failed = 0;

    if (argc != 2)
    {
        printf("usage: %s <extracted-disc-root>\n", argv[0]);
        return 2;
    }

    if (lom_native_cd_set_disc_root(argv[1]) != 0)
    {
        printf("Invalid extracted-disc root: %s\n", argv[1]);
        return 3;
    }

    cdrom_init();

    if (lom_native_load_resident(argv[1]) != 0)
    {
        return 4;
    }

    for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
    {
        s32 length = lengths[i];
        failed += check_normal(length, 0);
        failed += check_normal(0, length);
        failed += check_normal(length, length);
        failed += check_normal(length, -length * 2);
    }

    for (s32 angle = -4096; angle <= 4096; angle += 64)
    {
        failed += check_rotation((s16)angle);
    }

    printf("%d checks failed\n", failed);
    return failed == 0 ? 0 : 1;
}
