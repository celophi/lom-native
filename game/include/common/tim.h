#ifndef _TIM_H
#define _TIM_H

#include "common.h"

#include "common/gpu_packet.h"

#define TIM_HEADER_SIZE 8

#define TIM_FLAG_HAS_CLUT 8

#define TIM_FLAG_PIXEL_MODE_MASK 7

#define TIM_NEXT_BLOCK(block) ((TimBlock*)((u8*)(block) + (block)->bnum))

#define TIM_SKIP_BLOCK(block, bnum) ((TimBlock*)((u8*)(block) + (bnum)))

#define TIM_BLOCK_DATA(block) ((u_long*)((block) + 1))

#define TIM_FIRST_BLOCK(tim) ((TimBlock*)((u8*)(tim) + TIM_HEADER_SIZE))

#define TIM_BLOCK_RECT(block) (*(RECT*)&(block)->dx)

#define TIM_HAS_CLUT(tim) (*(u8*)&(tim)->flags & TIM_FLAG_HAS_CLUT)

#define TIM_PIXEL_MODE(tim) (*(u8*)&(tim)->flags & TIM_FLAG_PIXEL_MODE_MASK)

#define TIM_PIXEL_BLOCK(tim, bnum) ((TimBlock*)((u8*)(tim) + ((bnum) + TIM_HEADER_SIZE)))

#define CLUT_ENTRY_COUNT 0x100

typedef struct
{
    s16 pixel_x;
    s16 pixel_y;
    s16 clut_x;
    s16 clut_y;
} TimUploadDestination;

typedef struct
{
    u16 width;
    u16 height;
} TimDimensions;

typedef struct
{
    u32 bnum;
    u16 dx;
    u16 dy;
    TimDimensions dimensions;
} TimBlock;

typedef struct
{
    u32 magic;
    u32 flags;
    TimBlock clut_block;
    u16 clut_data[1];
} TimPrefix;

/**
 * @brief Upload a TIM's optional CLUT and its pixel data to the given VRAM positions.
 * @param destinations VRAM destinations for the pixel and CLUT blocks.
 * @param tim TIM resource to upload.
 */
void upload_tim_to_vram(TimUploadDestination* destinations, TimPrefix* tim);

typedef struct
{
    u32 magic;
    u32 flags;
    TimBlock clut_block;
    u16 clut_data[CLUT_ENTRY_COUNT];
} Tim;

#endif
