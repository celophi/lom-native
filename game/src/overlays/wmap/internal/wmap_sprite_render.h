#ifndef WMAP_SPRITE_RENDER_H
#define WMAP_SPRITE_RENDER_H

#include "common.h"

typedef struct
{
    s16 index;
    s16 resource_index;
    u8 unused_04[2];
    s8 scale_index;
    u8 unused_07[7];
    s16 sequence;
    s16 previous_sequence;
    u8 unused_12[2];
    u8* cursor;
    u8* sequence_start;
    u8* frame_data;
    s16 remaining;
    s16 target_shade;
    s16 shade;
    s16 shade_step;
    u8 unused_28[4];
} WmapSpriteActor;

#define WMAP_SPRITE_SCALE_FULL 15
#define WMAP_SPRITE_SHADE_NEUTRAL 0x80

#define WMAP_ANIMATION_SEQUENCES 32
#define WMAP_ANIMATION_HOLD 255
#define WMAP_ANIMATION_LOOP 255

typedef struct
{
    s32 index;
    u8* data;
} WmapAnimationSlot;

typedef union
{
    s32 packed;

    struct
    {
        s16 x;
        s16 y;
    } point;
} WmapScreenPosition;

typedef struct
{
    s16 x;
    s16 y;
} WmapPoint;

void wmap_draw_actor_sprite(WmapSpriteActor* actor, s32 screen_position, s32 texture_index, s32 ot_index, s32 variant);

#define WMAP_ACTOR_COUNT 256

extern WmapSpriteActor g_wmap_sprite_actors[WMAP_ACTOR_COUNT];

extern WmapAnimationSlot g_wmap_actor_animations[WMAP_ACTOR_COUNT];

#define WMAP_ACTOR_BLOCK(first) (&g_wmap_sprite_actors[first])

#define WMAP_VEHICLE_ACTOR 255

#if defined(LOM_NATIVE_POINTERS)
#define g_wmap_vehicle_actor (g_wmap_sprite_actors[WMAP_VEHICLE_ACTOR])
#define g_wmap_vehicle_animation (g_wmap_actor_animations[WMAP_VEHICLE_ACTOR])
#else
extern WmapSpriteActor g_wmap_vehicle_actor;
extern WmapAnimationSlot g_wmap_vehicle_animation;
#endif

/**
 * @brief Advance a sprite actor's animation by one frame and point it at the current frame's data.
 * @param actor Sprite actor to animate.
 * @param resource Animation resource: WMAP_ANIMATION_SEQUENCES sequence offsets, then frame offsets.
 * @note wmap_step_actor_animation does the same and also returns the new frame.
 */
static inline void wmap_advance_actor_animation(WmapSpriteActor* actor, u8* resource)
{
    s16* offsets;
    u8* data;
    u8* cursor;
    s32 frame;

    offsets = (s16*)resource;
    data = (u8*)offsets;

    if (actor->previous_sequence != actor->sequence)
    {
        s32 offset;
        u8* sequence_cursor;

        actor->previous_sequence = actor->sequence;
        offset = offsets[actor->sequence];
        actor->remaining = 1;
        sequence_cursor = (u8*)offsets + offset;
        actor->sequence_start = sequence_cursor;
        actor->cursor = sequence_cursor;
    }

    if (actor->remaining != WMAP_ANIMATION_HOLD)
    {
        actor->remaining--;
    }

    if (actor->remaining == 0)
    {
        cursor = actor->cursor;
        frame = cursor[0];
        actor->remaining = cursor[1];

        if (frame == WMAP_ANIMATION_LOOP)
        {
            cursor = actor->sequence_start;
            actor->cursor = cursor;
            frame = cursor[0];
            actor->remaining = cursor[1];
        }

        actor->cursor += 4;
        actor->frame_data = data + offsets[frame + WMAP_ANIMATION_SEQUENCES];
    }
}

#endif
