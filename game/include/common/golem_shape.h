#ifndef GOLEM_SHAPE_H
#define GOLEM_SHAPE_H

#include "common.h"

#define GOLEM_SHAPE_COUNT 11
#define GOLEM_SHAPE_ROTATION_COUNT 4
#define GOLEM_SHAPE_PART_COUNT 4

typedef struct
{
    s8 x;
    s8 y;
    s16 glyph_id;
} GolemShapePoint;

typedef struct
{
    GolemShapePoint origin;
    GolemShapePoint parts[GOLEM_SHAPE_PART_COUNT];
} GolemShapeRotation;

typedef struct
{
    u8 count;
    u8 reserved;
    u8 grid_width;
    u8 grid_height;
    s16 origin_x;
    s16 origin_y;
    GolemShapeRotation rotations[GOLEM_SHAPE_ROTATION_COUNT];
} GolemShape;

typedef struct
{
    GolemShape shapes[GOLEM_SHAPE_COUNT];
} GolemShapeTable;

extern GolemShape g_golem_shape_table[GOLEM_SHAPE_COUNT];

#endif
