#ifndef WMAP_EFFECT_PRIMITIVES_H
#define WMAP_EFFECT_PRIMITIVES_H

#include "common.h"
#include <libgte.h>

#include "wmap_sprite_render.h"

typedef struct
{
    s32 spawn_interval;
    s32 next_slot;
    s32 spawn_timer;
    s32 initial_angle;
    s32 radial_step;
    u16 angle_step;
    u8 unused_16[2];
    u16 shade_step;
    u8 unused_1a[2];
    u16 target_shade;
    u8 unused_1e[2];
    s32 depth;
    s32 texture_index;
} WmapTrailConfig;

typedef struct
{
    s32 count;
    s32 first;
    u16 x_min;
    u8 unused_0a[2];
    s32 x_range;
    u16 y_min;
    u8 unused_12[2];
    s32 y_range;
    u16 z_min;
    u8 unused_1a[2];
    s32 z_range;
    u16 target_shade;
    u8 unused_22[2];
    u16 shade;
    u8 unused_26[2];
    u16 shade_step;
    u8 unused_2a[2];
    s32 depth;
    s32 texture_index;
    u16 sequence;
    u8 unused_36[2];
} WmapBoxParticleConfig;

extern SVECTOR g_wmap_effect_camera_rotation_target;
extern SVECTOR g_wmap_effect_camera_rotation_step;
extern VECTOR g_wmap_effect_camera_translation_target;
extern VECTOR g_wmap_effect_camera_translation_step;
extern s32 g_wmap_effect_camera_move_active;

typedef struct
{
    s32 unused_00;
    s32 velocity_min;
    s32 velocity_range;
    s32 lifetime_min;
    s32 lifetime_range;
    s32 spawn_interval;
    s32 radial_step;
    s32 first_motion;
    s32 texture_index;
    s32 sequence;
    s32 radius;
} WmapParticleConfig;

#define WMAP_SET_PARTICLE_CONFIG(base, velocity_min, velocity_range, lifetime_min, lifetime_range, spawn_interval, radial_step, first_motion, texture_index,   \
                                 sequence, radius)                                                                                                             \
    (g_wmap_effect_params[(base) + 1] = (velocity_min), g_wmap_effect_params[(base) + 2] = (velocity_range),                                                   \
     g_wmap_effect_params[(base) + 3] = (lifetime_min), g_wmap_effect_params[(base) + 4] = (lifetime_range),                                                   \
     g_wmap_effect_params[(base) + 5] = (spawn_interval), g_wmap_effect_params[(base) + 6] = (radial_step), g_wmap_effect_params[(base) + 7] = (first_motion), \
     g_wmap_effect_params[(base) + 8] = (texture_index), g_wmap_effect_params[(base) + 9] = (sequence), g_wmap_effect_params[(base) + 10] = (radius))

#if defined(LOM_NATIVE_POINTERS)
#define WMAP_ANIMATION_SLOT_AT(base, offset) ((void)(base), (u8*)&g_wmap_actor_animations[(offset) / 8])
#define WMAP_MOTION_AT(base, offset) ((void)(base), (s16*)&g_wmap_actor_motions[(offset) / 0x14])
#else
#define WMAP_ANIMATION_SLOT_AT(base, offset) ((u8*)((offset) + (base)))
#define WMAP_MOTION_AT(base, offset) ((s16*)((offset) + (base)))
#endif

void wmap_update_particle_system(WmapSpriteActor* actors, WmapAnimationSlot* animations, s32 count, s32 peak_shade, s32 initial_shade, s32 shade_step, u32 mode,
                                 void* params);
void wmap_update_radial_burst(WmapSpriteActor* actors, WmapAnimationSlot* animations, s32 first, s32 end, s32 scale, s32 velocity_min, s32 velocity_range,
                              s32 lifetime_min, s32 lifetime_range, s32 initial_z, s32 frame, s32 spawn_interval);
void wmap_set_effect_model_transform(VECTOR* translation, SVECTOR* rotation);
void wmap_set_effect_camera_transform(void);
void wmap_update_flat_radial_burst(s32 first, s32 end, s32 frame, s32 depth, s32 velocity_base, s32 velocity_divisor, s32 lifetime_base, s32 sequence);
void wmap_update_falling_particles(s32 first, s32 end, s32 spawn_interval, s32 scale_override, s32 velocity_min, s32 velocity_range, s32 initial_z, s32 frame,
                                   s32 x_min, s32 x_range, s32 y_min, s32 y_range, s32 fade_z, s32 initial_scale_target, s32 initial_scale, s32 scale_step,
                                   s32 group);
void wmap_update_spiral_particles(s32 first, s32 end, s32 frame, s32 z_step, s32 depth);
void wmap_update_bouncing_particles(s32 first, s32 end, void* point_data, s32 frame, s32 depth);
void wmap_update_particle_trails(s32 first, s32 count, WmapTrailConfig* config, s32 expire_by_shade);
s32 wmap_run_effect_camera_move(s32 initialize);
void wmap_update_box_particles(WmapBoxParticleConfig* config);

extern MATRIX g_wmap_effect_camera_matrix;

extern s32* g_wmap_effect_params;

extern s32 g_wmap_particle_shade;

#endif
