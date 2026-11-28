#ifndef LOM_TEST_GPU_DRAW_H
#define LOM_TEST_GPU_DRAW_H

#include <psyz/gpu.h>
#include <stdint.h>

/**
 * @brief Have the GPU run primitives the game built, as it does when a frame's drawing is sent: each one's words, from
 *        its code word, go into the GPU's queue, then the queue runs. The Remaster's own commands, which the port
 *        turns the game's primitives into, are drawn through the HD renderer provider as they run.
 * @param code The first primitive's code word.
 * @param words How many words each one has from its code word on.
 * @param stride How far each next one's code word is, in bytes.
 * @param count How many primitives.
 */
static inline void gpu_draw(const void* code, int words, int stride, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const uint32_t* primitive = (const uint32_t*)((const unsigned char*)code + (long)i * stride);

        for (int w = 0; w < words; ++w)
        {
            Psyz_GpuWriteGP0(primitive[w]);
        }
    }

    Psyz_GpuExeque();
}

#endif
