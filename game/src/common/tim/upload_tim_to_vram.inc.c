#include "common/tim.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

/**
 * @brief Upload a TIM's optional CLUT and its pixel data to the given VRAM positions.
 * @param destinations VRAM destinations for the pixel and CLUT blocks.
 * @param tim TIM resource to upload.
 * @note The pixel data is always taken from after a CLUT block, so a TIM
 *       without one would upload the wrong bytes.
 */
void upload_tim_to_vram(TimUploadDestination* destinations, TimPrefix* tim)
{
    RECT upload_rect;
    s32 flags;
    s32 clut_block_size;
    TimDimensions* pixel_dimensions;

    flags = tim->flags;
    clut_block_size = tim->clut_block.bnum;

    if (flags & TIM_FLAG_HAS_CLUT)
    {
        lom_gpu_set_rect(&upload_rect, destinations->clut_x, destinations->clut_y, CLUT_ENTRY_COUNT, 1);
        lom_graphics_upload(&upload_rect, (u_long*)tim->clut_data);
        pixel_dimensions = &TIM_PIXEL_BLOCK(tim, clut_block_size)->dimensions;
    }
    else
    {
        pixel_dimensions = &tim->clut_block.dimensions;
    }

    lom_gpu_set_rect(&upload_rect, destinations->pixel_x, destinations->pixel_y, pixel_dimensions->width, pixel_dimensions->height);
    lom_graphics_upload(&upload_rect, (u_long*)(TIM_PIXEL_BLOCK(tim, clut_block_size) + 1));
}
