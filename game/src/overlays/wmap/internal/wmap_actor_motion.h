#ifndef WMAP_ACTOR_MOTION_H
#define WMAP_ACTOR_MOTION_H

#include "common.h"

#include "wmap_sprite_render.h"

typedef union
{
    s32 value;
    s16 low;
} WmapAngularVelocity;

typedef struct
{
    s16 state;
    s16 angle;
    s32 velocity;
    s32 radius;
    s16 lifetime;
    s16 z;
    WmapAngularVelocity angular_velocity;
} WmapMotion;

typedef struct
{
    s16 state;
    s16 angle;
    s32 velocity;
    s32 radius;
    s16 lifetime;
    s16 z;
    WmapScreenPosition screen_position;
} WmapTrailMotion;

extern WmapMotion g_wmap_actor_motions[WMAP_ACTOR_COUNT];

extern WmapMotion g_wmap_actor_motions_at_15[];
extern WmapMotion g_wmap_actor_motions_at_50[];
extern WmapMotion g_wmap_actor_motions_at_120[];
extern WmapMotion g_wmap_actor_motions_at_200[];

extern WmapMotion g_wmap_effect_actor_motions[];

extern WmapMotion g_wmap_actor_motions_at_10[];

extern WmapMotion g_wmap_actor_motions_at_60[];

#endif
