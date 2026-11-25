#ifndef WMAP_CELLS_H
#define WMAP_CELLS_H

#include "common.h"

#define WMAP_GRID_SIZE 6

#define WMAP_NO_LAND 0xFF

#define WMAP_LAND_FULL_SIZE 0x100

#define WMAP_CELL_SPACING 48

typedef struct
{
    s32 land_id;
    s16 placement_allowed;
    s16 travel_allowed;
    s32 spirit_sprites[8];
} WmapCell;

extern WmapCell g_wmap_cells[WMAP_GRID_SIZE][WMAP_GRID_SIZE];

typedef struct
{
    s16 unused_00;
    s16 mode;
    s16 state;
    s16 frame;
    s32 unused_08;
    s32 unused_0c;
    s16 unused_10;
    u8 pad12[2];
    s32 unused_14;
    s32 unused_18;
} WmapEffectCell;

extern WmapEffectCell g_wmap_cell_effects[6][6];

#endif
