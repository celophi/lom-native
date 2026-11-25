#ifndef WMAP_SEQUENCE_RUNTIME_H
#define WMAP_SEQUENCE_RUNTIME_H

#include "common.h"
#include <libgte.h>

#include "wmap_sprite_render.h"

typedef s32 (*WmapSequenceCallback)(s32 initialize);

typedef void (*WmapSequenceStep)(void);

extern s32 g_wmap_sequence_busy;

s32 wmap_run_land_focus(s32 reset);
void wmap_update_sequences(void);
void wmap_start_sequence(WmapSequenceCallback callback);
void wmap_update_callbacks(void);
void wmap_install_callback(WmapSequenceCallback callback);
s32 wmap_step_actor_animation(void* actor_data, void* resource_data);
void wmap_init_sequences(void);
void wmap_draw_model_default(u8* resource_table, s32 resource_index, s32 ot_index, s32 tpage, s32 clut, s32 blend_mode, s32 color_scale);
void wmap_project_focus_cell(void);
s32 wmap_scale_color(CVECTOR color, s32 scale);
void wmap_set_model_transform(VECTOR* translation, SVECTOR* rotation);
void wmap_update_default_particles_from_settings(void* actors, WmapAnimationSlot* animations, s32 count, s32 peak_shade, s32 initial_shade, s32 shade_step);
void wmap_update_particles_from_settings(void* actors, WmapAnimationSlot* animations, s32 count, s32 peak_shade, s32 initial_shade, s32 shade_step, s32 mode);
s32 wmap_find_land_cell(s32 value, s32* row_out, s32* column_out);
void wmap_set_map_rotation(SVECTOR* rotation);
void wmap_reset_focus_screen_position(void);
s32 wmap_run_land_entry(s32 reset);

extern SVECTOR g_wmap_zero_rotation;
extern VECTOR g_wmap_zero_translation;

extern SVECTOR g_wmap_effect_model_a_rotation;
extern VECTOR g_wmap_effect_model_a_position;
extern SVECTOR g_wmap_effect_model_b_rotation;
extern VECTOR g_wmap_effect_model_b_position;
extern SVECTOR g_wmap_effect_model_c_rotation;
extern VECTOR g_wmap_effect_model_c_position;
extern SVECTOR g_wmap_effect_model_d_rotation;
extern VECTOR g_wmap_effect_model_d_position;
extern SVECTOR g_wmap_effect_model_e_rotation;
extern SVECTOR g_wmap_effect_model_f_rotation;
extern SVECTOR g_wmap_effect_model_g_rotation;
extern VECTOR g_wmap_effect_model_g_position;
extern SVECTOR g_wmap_effect_model_h_rotation;
extern VECTOR g_wmap_effect_model_h_position;
extern s32 g_wmap_effect_fade_a;
extern s32 g_wmap_effect_fade_b;
extern s32 g_wmap_effect_fade_c;
extern s32 g_wmap_effect_fade_d;
extern s32 g_wmap_aux_effect_fade_a;
extern s32 g_wmap_aux_effect_fade_b;

extern s32 g_wmap_focus_cell_x;
extern s32 g_wmap_focus_cell_y;
extern s32 g_wmap_event_active;
extern s32 g_wmap_placement_overlay_hidden;
extern s32 g_wmap_transition_mesh_hidden;
extern s32 g_wmap_frame_count;
extern s32 g_wmap_particle_intensity;

extern s32 g_wmap_falling_spawn_timers[4];
extern s32 g_wmap_falling_spawn_counts[4];

extern s32 g_wmap_particle_velocity_min;
extern s32 g_wmap_particle_velocity_range;
extern s32 g_wmap_particle_lifetime_min;
extern s32 g_wmap_particle_lifetime_range;
extern s32 g_wmap_particle_spawn_interval;
extern s32 g_wmap_particle_radial_step;
extern s32 g_wmap_particle_first_motion;
extern s32 g_wmap_particle_texture_index;
extern s32 g_wmap_particle_sequence;
extern s32 g_wmap_particle_radius;

#define WMAP_SET_PARTICLE_SETTINGS(velocity_min, velocity_range, lifetime_min, lifetime_range, spawn_interval, radial_step, first_motion, texture_index,       \
                                   sequence, radius)                                                                                                           \
    (g_wmap_particle_velocity_min = (velocity_min), g_wmap_particle_velocity_range = (velocity_range), g_wmap_particle_lifetime_min = (lifetime_min),          \
     g_wmap_particle_lifetime_range = (lifetime_range), g_wmap_particle_spawn_interval = (spawn_interval), g_wmap_particle_radial_step = (radial_step),        \
     g_wmap_particle_first_motion = (first_motion), g_wmap_particle_texture_index = (texture_index), g_wmap_particle_sequence = (sequence),                    \
     g_wmap_particle_radius = (radius))

extern s32 g_wmap_effect_fade_e;
extern s32 g_wmap_effect_fade_f;
extern s32 g_wmap_aux_effect_fade_c;
extern s32 g_wmap_effect_flash_fade_a;
extern s32 g_wmap_effect_flash_fade_b;
extern s32 g_wmap_effect_wave_phase;
extern s32 g_wmap_effect_model_frame;

extern s32 g_wmap_particle_arc_apex_lifetime;
extern s32 g_wmap_particle_arc_peak;
extern s32 g_wmap_particle_arc_divisor;
extern s32 g_wmap_particle_arc_base;
extern s32 g_wmap_particle_fall_range;

extern WmapScreenPosition g_wmap_effect_sprite_position_a;
extern WmapScreenPosition g_wmap_effect_sprite_position_b;
extern WmapScreenPosition g_wmap_effect_sprite_position_c;
extern s32 g_wmap_carousel_artifacts[];
extern s32 g_wmap_models_use_effect_camera;
extern s32 g_wmap_unused_tally;
extern s8 g_wmap_cosine_wave[512];
extern s32 g_wmap_sequence_count;

#endif
