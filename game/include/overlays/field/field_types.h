#ifndef _FIELD_TYPES_H
#define _FIELD_TYPES_H

#include "common.h"
#include "common/vector.h"
#include <libgte.h>

typedef struct
{
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} FieldVector;

#define FIELD_ANGLE_TURN 0x1000
#define FIELD_ANGLE_HALF_TURN 0x800
#define FIELD_ANGLE_QUARTER_TURN 0x400
#define FIELD_ANGLE_MASK 0xFFF

#endif
