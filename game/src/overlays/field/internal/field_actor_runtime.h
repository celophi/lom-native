#ifndef FIELD_ACTOR_RUNTIME_H
#define FIELD_ACTOR_RUNTIME_H

#include "common.h"

struct FieldActor;
struct FieldActorSlot;
struct FieldRenderHalf;
struct FieldRenderContext;

#define FIELD_FRAME_DATA_8BIT 0x80000000u

#if defined(LOM_NATIVE_POINTERS)
#if UINTPTR_MAX > UINT32_MAX
#define FIELD_FRAME_DATA_16BIT_MARK ((uintptr_t)1 << 63)
#else
#define FIELD_FRAME_DATA_16BIT_MARK ((uintptr_t)1 << 31)
#endif
#define FIELD_FRAME_DATA_MARK_16BIT(address) ((u8*)((uintptr_t)(address) | FIELD_FRAME_DATA_16BIT_MARK))
#define FIELD_FRAME_DATA_IS_16BIT(address) (((uintptr_t)(address) & FIELD_FRAME_DATA_16BIT_MARK) != 0)
#define FIELD_FRAME_DATA_ADDRESS(address) ((u8*)((uintptr_t)(address) & ~FIELD_FRAME_DATA_16BIT_MARK))
#else
#define FIELD_FRAME_DATA_MARK_16BIT(address) ((u8*)((uintptr_t)(address) & ~FIELD_FRAME_DATA_8BIT))
#define FIELD_FRAME_DATA_IS_16BIT(address) (!((uintptr_t)(address) & FIELD_FRAME_DATA_8BIT))
#define FIELD_FRAME_DATA_ADDRESS(address) ((u8*)(address))
#endif

#if defined(LOM_NATIVE_POINTERS)
#define FIELD_FRAME_DATA_POSITION(frame_data) ((uintptr_t)FIELD_FRAME_DATA_ADDRESS(frame_data))
#define FIELD_RESOURCE_POSITION(start) ((uintptr_t)(start))
#else
#define FIELD_FRAME_DATA_POSITION(frame_data) ((frame_data) & 0x001FFFFFu)
#define FIELD_RESOURCE_POSITION(start) ((u32)(uintptr_t)(start) & 0x001FFFFFu)
#endif

#define FIELD_SOUND_EVENT_START 1
#define FIELD_SOUND_EVENT_PART_SPAWN 2
#define FIELD_SOUND_EVENT_EFFECT_RETIRED 3
#define FIELD_SOUND_EVENT_EFFECT_BOUNCE 4
#define FIELD_SOUND_EVENT_PART_SPAWN_ONCE 5

#define FIELD_TRACK_INDEX_MASK 0xF

#define FIELD_WEAPON_SET_FIELD 0
#define FIELD_WEAPON_SET_BATTLE 1

#define FIELD_ANIMATION_BATTLE_ENTRY 0x11
#define FIELD_ANIMATION_BATTLE_EXIT 0x12
#define FIELD_PARTY_DEFEAT_ANIMATION 0x2F

void field_update_dialog_runtime(struct FieldRenderHalf* render_half);
void field_show_battle_results(void);
void field_close_dialog_screen(void);
void field_request_return_to_title(s32 command_value);
void field_begin_gover_transition(s32 image_resource_index, s32 music_resource_index, s32 audio_clip_index);
void field_update_gover_load(void);
void field_update_return_to_title_prompt(struct FieldRenderHalf* render_half);
void field_open_return_to_title_prompt(void);
void field_begin_return_to_title_prompt_close(void);
s32 field_get_track_counter_modulo(struct FieldActorSlot* slot, s32 divisor);
void field_initialize_actor_slots(void);
void field_clear_actor_slots(void);
void field_start_actor_animation(s32 slot_index, int target_count, u8* targets);
s32 field_is_actor_animation_active(s32 slot_index);
s32 field_find_active_special_attack_actor(void);
void field_update_actor_animations(void);
void field_prepare_actor_render_commands(struct FieldRenderContext* render_context, s32 unused);
void field_reset_global_color_scale(void);
void field_reset_actor_resource_slots(void);
void field_initialize_actor_system(void);
void field_initialize_actor_parts(s32 timer_mode);
void field_release_actor_resource_slot(s32 slot_index_minus_one);
s32 field_activate_actor_resource_slot(s32 source_selector, s32 resource_variant, s32 slot_index_minus_one);
void field_find_or_load_resource_entry(s32 resource_slot_id, s32 package);
void field_set_all_actor_render_state(s32 red, s32 green, s32 blue, s32 color_flag, s32 blend_mode);
s32 field_set_actor_render_state(s32 red, s32 green, s32 blue, s32 color_flag, s32 blend_mode, s32 actor_selector);
void field_update_actor_objects(void);
s32 field_get_actor_sound_pan(s32 actor_index);
void field_restart_actor_animation_reverse(struct FieldActor* actor);
s32 field_get_next_animation_frame_count(struct FieldActor* actor);
void field_reset_effect_pool(void);
s32 field_evaluate_parameter_track(struct FieldActorSlot* slot, s32 curve_index);

typedef struct FieldDirectionOffset
{
    s16 dx;
    s16 dz;
} FieldDirectionOffset;

extern s32 g_field_gover_load_countdown;

extern s32 g_field_return_to_title_prompt_state;

extern s32 g_field_return_to_title_prompt_delay;

extern s32 g_field_track_index;

extern u8* g_field_resource_cursor;

extern FieldDirectionOffset g_field_direction_offsets[];

#endif
