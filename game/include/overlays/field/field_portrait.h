#ifndef FIELD_PORTRAIT_H
#define FIELD_PORTRAIT_H

#include "common.h"
#include "common/gpu_packet.h"

#define FIELD_PORTRAIT_SIZE 48

#define FIELD_PORTRAIT_BYTES (GPU_CLUT_4BIT_COLORS * 2 + FIELD_PORTRAIT_SIZE * FIELD_PORTRAIT_SIZE / 2)

typedef struct
{
    u16 clut[GPU_CLUT_4BIT_COLORS];
    u8 pixels[FIELD_PORTRAIT_SIZE][FIELD_PORTRAIT_SIZE / 2];
} FieldPortrait;

#define FIELD_PORTRAIT_PIXELS_OFFSET (GPU_CLUT_4BIT_COLORS * 2)

#define FIELD_PARTY_PORTRAIT_COUNT 3
#define FIELD_PARTY_PORTRAIT_CLUT_X 272
#define FIELD_PARTY_PORTRAIT_CLUT_Y 472
#define FIELD_PARTY_PORTRAIT_VRAM_X 1012
#define FIELD_PARTY_PORTRAIT_VRAM_X2 1000
#define FIELD_PARTY_PORTRAIT_VRAM_Y0 288
#define FIELD_PARTY_PORTRAIT_VRAM_Y1 336

/**
 * @brief Copy one of the portrait palettes into a portrait image.
 * @param dest Palette strip at the start of the portrait image.
 * @param index Palette index.
 */
void field_copy_portrait_palette(void* dest, s32 index);

/**
 * @brief Copy a golem portrait palette into a portrait image.
 * @param destination Palette strip at the start of the portrait image.
 * @param palette Golem palette index (0-31).
 */
void field_copy_golem_portrait_palette(u8* destination, s32 palette);

#endif
