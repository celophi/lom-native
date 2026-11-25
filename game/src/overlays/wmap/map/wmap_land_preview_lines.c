#include "../internal/wmap_land_preview_lines.h"
#include <libgpu.h>
#include "lom_native_gpu_packets.h"

#include "common/gpu_packet.h"

extern LINE_F2 g_wmap_preview_lines[16];

/**
 * @brief Empty world-map handler (no operation).
 */
void wmap_land_preview_stub(void)
{
}

/** @brief Initialize 16 black, semitransparent two-point line packets. */
void wmap_init_preview_lines(void)
{
    s32 index;

    for (index = 0; index < 16; index++)
    {
        SET_BGR0_PACKED(&g_wmap_preview_lines[index], 0);
        lom_gpu_set_length(&g_wmap_preview_lines[index], 3);
        lom_gpu_set_code(&g_wmap_preview_lines[index], 0x42);
    }
}
