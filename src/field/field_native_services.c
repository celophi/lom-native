#include "common.h"
#include <libgpu.h>
#include <libgte.h>
#include "sdk/gte_dmpsx_compat.h"
#include "lom_native_unit_decode.h"
#include "lom_native_field.h"
#include "lom_native_overlay_entry.h"
#include "lom_native_cd.h"
#include "main/cdrom.h"
#include "../movie/field_movie_native.h"

#include <stdio.h>

#pragma weak lom_native_field_music_drain

/**
 * @brief Find one of the main executable's tables, by its PS1 address.
 *        The Japanese executable has the same square root and sine tables
 *        as the US one, 0x16C bytes further on.
 * @param address The PS1 address.
 * @return Where it is in the port's memory.
 */
static const void* resident_table(unsigned int address)
{
    return lom_unit_host_address(address, "a main executable table");
}

#if defined(VERSION_JP)
#define PS1_RSQRT_TABLE_ADDRESS 0x80036FB0u
#define PS1_SINCOS_TABLE_ADDRESS 0x8003744Cu
#else
#define PS1_RSQRT_TABLE_ADDRESS 0x80036E44u
#define PS1_SINCOS_TABLE_ADDRESS 0x800372E0u
#endif

#define PS1_RSQRT_ENTRY(offset) resident_table(PS1_RSQRT_TABLE_ADDRESS + (u32)(offset))
#define PS1_SINCOS_TABLE ((uintptr_t)resident_table(PS1_SINCOS_TABLE_ADDRESS))

/**
 * @brief Psy-Q's ApplyMatrixLV, which PSY-Z's doesn't match: multiply a long
 *        vector by the rotation matrix with the GTE, in two halves so its high
 *        bits aren't lost.
 * @param matrix The matrix.
 * @param input The vector.
 * @param output Set to the product.
 * @return @p output.
 */
VECTOR* ApplyMatrixLV(MATRIX* matrix, VECTOR* input, VECTOR* output)
{
    s32 source[3];
    u32 high[3];
    __builtin_memcpy(source, input, sizeof(source));
    SetRotMatrix(matrix);

    for (unsigned i = 0; i < 3; ++i)
    {
        Psyz_GteDataWrite(9 + i, source[i] / 32768);
    }

    Psyz_GteCommand(0x041E012);

    for (unsigned i = 0; i < 3; ++i)
    {
        high[i] = Psyz_GteDataRead(25 + i) << 3;
    }

    for (unsigned i = 0; i < 3; ++i)
    {
        Psyz_GteDataWrite(9 + i, source[i] % 32768);
    }

    Psyz_GteCommand(0x049E012);
    u32 result[3];

    for (unsigned i = 0; i < 3; ++i)
    {
        result[i] = high[i] + Psyz_GteDataRead(25 + i);
    }

    __builtin_memcpy(output, result, sizeof(result));
    return output;
}

long lom_native_vector_normal(VECTOR* input, VECTOR* output)
{
    s32 source[3];
    __builtin_memcpy(source, input, sizeof(source));
    gte_ldlvl(source);
    gte_sqr0();
    u32 sum = Psyz_GteDataRead(25) + Psyz_GteDataRead(26) + Psyz_GteDataRead(27);
    Psyz_GteDataWrite(30, sum);
    u32 leading = Psyz_GteDataRead(31) & ~1u;
    s32 shift = (31 - (s32)leading) >> 1;
    s32 amount = (s32)leading - 24;
    u32 scaled = amount >= 0 ? sum << (amount & 31) : (u32)((s32)sum >> ((-amount) & 31));
    s16 factor;
    __builtin_memcpy(&factor, PS1_RSQRT_ENTRY((scaled - 64) << 1), 2);
    Psyz_GteDataWrite(8, (s32)factor);
    gte_ldlvl(source);
    Psyz_GteCommand(0x190003D);
    s32 result[3];

    for (unsigned i = 0; i < 3; ++i)
    {
        result[i] = (s32)Psyz_GteDataRead(25 + i) >> (shift & 31);
    }

    __builtin_memcpy(output, result, sizeof(result));
    return (s32)sum;
}

s32 lom_native_field_clock_card_type(s32 port)
{
    return 0;
}

/**
 * @brief Psy-Q's MoveImage2, the same as MoveImage.
 * @param rect The VRAM to copy.
 * @param x Where to, across.
 * @param y Where to, down.
 * @return What MoveImage returns.
 */
s32 MoveImage2(RECT* rect, s32 x, s32 y)
{
    return MoveImage(rect, x, y);
}

/**
 * @brief Stop FIELD because a file it asked for isn't on the extracted disc.
 * @param resource The file's resource index.
 */
static _Noreturn void field_cd_failure(s32 resource)
{
    char message[128];
    snprintf(message, sizeof(message), "resource %04X unavailable from extracted disc", (u32)resource);
    lom_native_field_fail(message);
}

/**
 * @brief FIELD's hook before each CD call: finish the music stream's reads
 *        first, and close the movie that's playing before reading one of
 *        the maps (MAP/MAP0 to MAP/MAP4).
 * @param call Which call it is.
 * @param resource The file's resource index.
 */
static void field_cd_before(LomNativeCdCall call, int32_t resource)
{
    if (lom_native_field_music_drain)
    {
        lom_native_field_music_drain();
    }

    if (call != LOM_NATIVE_CD_CALL_WAIT && resource >= 180 && resource <= 669)
    {
        lom_native_field_movie_shutdown();
    }
}

/**
 * @brief FIELD's hook after each CD call: stop FIELD when a read fails, or a
 *        wait fails or is blocked by a callback read nobody took.
 * @param call Which call it was.
 * @param resource The file's resource index.
 * @param result What the call returned.
 */
static void field_cd_after(LomNativeCdCall call, int32_t resource, int32_t result)
{
    if (call != LOM_NATIVE_CD_CALL_WAIT)
    {
        if (result < 0)
        {
            field_cd_failure(resource);
        }

        return;
    }

    LomNativeCdError error = lom_native_cd_get_last_error();

    if (error == LOM_NATIVE_CD_ERROR_CALLBACK_BLOCKED)
    {
        lom_native_field_fail("CD wait blocked by unconsumed callback sector");
    }

    if (error != LOM_NATIVE_CD_ERROR_NONE)
    {
        char message[96];
        snprintf(message, sizeof(message), "CD wait failed (error %d)", (int)error);
        lom_native_field_fail(message);
    }
}

const LomNativeCdHooks lom_native_field_cd_hooks = {field_cd_before, field_cd_after};

/**
 * @brief Multiply three numbers by a fourth, in 4.12 fixed point, with the GTE,
 *        as Psy-Q's RotMatrix does.
 * @param factor The fourth.
 * @param x The first.
 * @param y The second.
 * @param z The third.
 * @param out Set to the three products.
 */
static void rotation_product(s32 factor, s32 x, s32 y, s32 z, s32 out[3])
{
    Psyz_GteDataWrite(8, factor);
    Psyz_GteDataWrite(9, x);
    Psyz_GteDataWrite(10, y);
    Psyz_GteDataWrite(11, z);
    Psyz_GteCommand(0x198003D);

    for (unsigned i = 0; i < 3; ++i)
    {
        out[i] = (s32)Psyz_GteDataRead(9 + i);
    }
}

/**
 * @brief Psy-Q's RotMatrix_gte: build a rotation matrix from three angles, with
 *        the main executable's sine table and the GTE, as the original does.
 * @param angles The angles about X, Y and Z, 4096 to a turn.
 * @param matrix Set to the rotation.
 * @return @p matrix.
 */
MATRIX* RotMatrix_gte(SVECTOR* angles, MATRIX* matrix)
{
    s16 rotations[3];
    __builtin_memcpy(rotations, angles, sizeof(rotations));
    s32 sine[3], cosine[3];

    for (unsigned i = 0; i < 3; ++i)
    {
        s32 angle = rotations[i];
        unsigned index = (angle < 0 ? -angle : angle) & 0xFFF;
        s16 pair[2];
        __builtin_memcpy(pair, (void*)(PS1_SINCOS_TABLE + index * 4), sizeof(pair));
        sine[i] = angle < 0 ? -pair[0] : pair[0];
        cosine[i] = pair[1];
    }

    s32 cx[3], sx[3], cz[3], sz[3];
    rotation_product(cosine[0], sine[1], sine[2], cosine[2], cx);
    rotation_product(sine[0], sine[1], sine[2], cosine[2], sx);
    rotation_product(cosine[2], cosine[1], sx[0], cx[0], cz);
    rotation_product(sine[2], cosine[1], sx[0], cx[0], sz);
    matrix->m[0][0] = cz[0];
    matrix->m[0][1] = -sz[0];
    matrix->m[0][2] = sine[1];
    matrix->m[1][0] = cz[1] + cx[1];
    matrix->m[1][1] = cx[2] - sz[1];
    matrix->m[1][2] = -((cosine[1] * sine[0]) >> 12);
    matrix->m[2][0] = sx[1] - cz[2];
    matrix->m[2][1] = sx[2] + sz[2];
    matrix->m[2][2] = (cosine[1] * cosine[0]) >> 12;
    return matrix;
}

void lom_native_field_script_handler(void* handler)
{
    (void)handler;
}
