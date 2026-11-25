#ifndef _SCENE_STATE_H
#define _SCENE_STATE_H

#include "common.h"

typedef struct
{
    u16 map_id;
    u16 object_index;
    s32 camera_x;
    s32 camera_y;
    s32 camera_z;
    u32 pixel_lookup_selector;
} SceneState;

#define SCENE_STATE ((SceneState*)SCENE_STATE_ADDRESS)

#endif
