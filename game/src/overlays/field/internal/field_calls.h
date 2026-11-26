#ifndef FIELD_CALLS_H
#define FIELD_CALLS_H

#include "common.h"

#include "common/card_directory.h"
#include "common/tim.h"
#include "common/vector.h"
#include "main/scene_state.h"
#include "field_effect_dispatch.h"
#include "field_progression_ops.h"
#include "field_stat_counter_ops.h"
#include "overlays/field/field_fade.h"
#include "overlays/field/field_inventory.h"
#include "overlays/field/field_portrait.h"
#include "overlays/field/field_sound.h"

struct AkaoHeader;

struct FieldActionBank;
struct FieldScriptState;
struct FieldActionDescriptor;
struct FieldActor;
struct FieldActionSlot;
struct FieldActorRecord;
struct FieldActorSlot;
struct FieldPlayerRecord;
struct FieldActorSlot;
struct FieldCardClock;
struct FieldCdBuffer;
struct FieldCharacterRecord;
struct FieldActor;
struct FieldKeyedList;
struct FieldCollisionNode;
struct FieldItemKey;
struct FieldItemRecord;
struct ShopEntry;
struct FieldMotionRecord;
struct FieldPathPoint;
struct FieldBattleAction;
struct FieldObjectPart;
struct FieldNode;
struct FieldRenderHalf;
struct FieldStatusRecord;
struct FieldObjectState;

typedef struct FieldCollisionQuery
{
    s32 x;
    s32 y;
    s32 z;
    u16 width;
    s16 height_tolerance;
    u16 depth;
} FieldCollisionQuery;

#define FIELD_FOOTPRINT_WIDTH 9
#define FIELD_FOOTPRINT_DEPTH 6
#define FIELD_FOOTPRINT_LARGE_WIDTH 12
#define FIELD_FOOTPRINT_LARGE_DEPTH 8
#define FIELD_FOOTPRINT_STEP 16

typedef struct FieldCollisionMover
{
    s32 x;
    s32 height;
    s32 z;
    s32 move_x;
    s32 move_height;
    s32 move_z;
    s32 resolved_height;
    struct FieldCollisionNode* collision_node;
    s32 flags;
    u16 footprint_width;
    s16 height_bias;

    union
    {
        s32 mode_flags;
        s16 footprint_depth;

        struct
        {
            unsigned footprint_depth : 16;
            unsigned airborne_low : 1;
            unsigned airborne_high : 1;
            unsigned unused : 14;
        } bits;
    } mode;
} FieldCollisionMover;

#define FIELD_COLLISION_NODE_SEARCH ((struct FieldCollisionNode*)(uintptr_t)(u32) - 1)
#define FIELD_COLLISION_NODE_DETACHED ((struct FieldCollisionNode*)(uintptr_t)(u32) - 2)

#define FIELD_COLLISION_MOVER_SETTLED 0x1
#define FIELD_COLLISION_MOVER_AIRBORNE_LOW 0x10000
#define FIELD_COLLISION_MOVER_AIRBORNE_HIGH 0x20000
#define FIELD_COLLISION_MOVER_AIRBORNE (FIELD_COLLISION_MOVER_AIRBORNE_LOW | FIELD_COLLISION_MOVER_AIRBORNE_HIGH)

#define FIELD_COLLISION_RESULT_BLOCKED 0x1
#define FIELD_COLLISION_RESULT_SLID 0x2
#define FIELD_COLLISION_RESULT_SLOWED 0x4
#define FIELD_COLLISION_RESULT_CARRIED 0x10
#define FIELD_COLLISION_RESULT_HEIGHT 0x20
#define FIELD_COLLISION_RESULT_CEILING 0x40
#define FIELD_COLLISION_RESULT_LANDED 0x80

#define FIELD_COLLISION_MOVER ((FieldCollisionMover*)SCRATCHPAD_ADDRESS)

typedef struct FieldPos
{
    s16 x;
    s16 y;
    s16 z;
} FieldPos;

extern struct FieldActionBank* g_field_action_bank;
struct FieldActionDescriptor* field_select_action_descriptor(void);

void field_battle_set_watched_record(s32 record_id);
s32 field_battle_handle_defeat(struct FieldStatusRecord* record);
void field_battle_finish(s32 result);
void field_battle_defeat_record(s32 record_id);
s32 field_run_action_handler(void);
void field_select_coordinate_labels(void);
/**
 * @brief field_battle_run_party_event(void).
 * @note Declared without parameters: field_action_descriptors.c passes an argument.
 */
void field_battle_run_party_event();

s32 field_battle_resolve_action(struct FieldBattleAction* action);
s32 field_battle_roll_evasion(struct FieldBattleAction* action);

s32 field_join_companion(void);
s32 field_join_golem(s32 logic_type);
s32 field_join_guest(s32 guest_id);
void field_leave_party(s32 companion);
s32 field_rejoin_companion(void);

void field_reset_action_command_map(s32 map_index);

extern s32 g_field_direction_animation_modes[];
extern s32 g_field_actor_walk_animations[];

void field_camera_track_party(void);
void field_camera_update(void);
void field_camera_select_scroll_limits(void);
void field_camera_reset(void);

void field_start_object_ground_effect(struct FieldActor* actor, u32 kind);
void field_draw_object_ground_effect(struct FieldActor* actor, u32 kind);

void field_update_spawned_actor(struct FieldActor* actor);
void field_draw_actor_hud(struct FieldRenderHalf* render_half);
void field_update_object_effects(s32 index);
void field_clear_fade_prims(void);
void field_add_fade_prim(const void* prim, s16 depth);
s32 field_load_vram_resource(s32 id, RECT* rect, s32 mode);
s32 field_upload_image_resource(RECT* rect, Tim* resource, s32 mode);
void* field_emit_actor_portrait(SPRT* sprt, OT_TYPE* ot, s32 index, Vec2s* position);
void field_draw_fade_prims(struct FieldRenderHalf* render_half);
extern s32 g_field_party_hud_order[];
s32 field_is_actor_idle(s32 key);
u32* field_get_scene_record_table(void);
POLY_FT4* field_render_actor_ground_shadow(struct FieldActor* actor, POLY_FT4* prim, GPU_OT_ENTRY(s32) * ordering_table, Vec2s* footprint);

void field_update_hit_reaction(struct FieldActor* actor);
void field_idle_actor_after_animation(struct FieldActor* actor);
void field_idle_actor_after_reload(struct FieldActor* actor);

s32 field_get_held_action_buttons(s32 player, s32 action, struct FieldActor* actor);
void field_poll_leader_interaction(void);
s32 field_resolve_action_command(struct FieldActor* actor, s32 player);
void field_update_actor_run_button(struct FieldActor* actor, s32 player);

void field_reset_action_command_maps(void);
void field_apply_weapon_action_params(s32 player_index);

s32 field_get_actor_position(s32 key, Vec3i* position);
s32 field_set_actor_position(s32 key, s32 x, s32 y, s32 z);
s32 field_set_actor_control_mode(s32 key, s32 mode);
struct FieldActor* field_lookup_actor(s32 key);
void field_set_actor_group(s32 key, s32 group);
s32 field_actor_faces_actor(s32 first_key, s32 second_key);
s32 field_get_actor_binding_state(s32 key);
void field_reload_actor(s32 key, s32 resource_entry_index, s32 resource_slot_id, s32 package, s32 group, s32 x, s32 y, s32 z, s32 animation, s32 resource_flag);
u8* field_get_event_script(s32 index);
void field_reset_actor_at(s32 key, s32 resource_entry_index, s32 group, s32 x, s32 y, s32 z);
s32 field_start_actor_private_script(s32 key, u8* script);
struct FieldObjectState* field_find_object_state(s32 key);

struct FieldActorTemplate;
struct FieldActorTemplateTable;
void field_battle_scan_actor_objects(void);
void field_battle_start(s32 group);
void field_battle_suspend(void);
void field_battle_end(void);
struct FieldActorTemplate* field_find_actor_template(struct FieldActorTemplateTable* table, s32 id);

void field_update_actor_action(struct FieldActor* actor, s32 update_action);
s32 field_move_leaves_screen(struct FieldActor* actor, Vec3i* delta);

s32 field_stop_actor(s32 key);
s32 field_test_actor_depth_overlap(s32 first_key, s32 second_key);
void field_release_object_link(struct FieldActor* actor);
void field_collapse_defeated_actor(struct FieldActor* actor);
void field_knock_down_actor(struct FieldActor* actor, s32 clear_recovery);
void field_start_actor_hit_reaction(struct FieldActor* actor, s32 guard);
void field_count_chain_hit(s32 object_index);
void field_start_actor_jump(struct FieldActor* actor, s32 direction, s32 animation, s32 command_param);
void field_start_actor_defeat(struct FieldActor* actor, s32 value);

void field_spawn_item_record(s32 key, s32 item);
s32 field_find_nearest_faced_item(s32 unused);
void field_set_actor_record_script_only(s32 key, s32 flags);
void field_clear_actor_record_script_only(s32 key);

void field_check_marker_contact(struct FieldActor* player);
s32 field_request_resource_read(s32 resource_id);
void field_clear_resource_queue(void);
s32 field_get_loading_resource(void);
void field_issue_next_resource_read(void);
void field_free_owner_resources(s32 tag);
void field_unpack_actor_resource(s32 owner, struct FieldActorSlot* actor);
extern s16 g_field_texture_slot_flags[];

void field_restore_default_action_animation_mappings(s32 unused);
void field_clear_actor_effects(struct FieldActorSlot* slot);
s32 field_get_actor_resource_id(s32 unused_slot_index, struct FieldPlayerRecord* player, s32 weapon_set);
void field_initialize_actor_part(s32 part_index, s32 timer_mode);
void field_initialize_actor_record(s32 actor_index, s32 resource_entry_index);
void field_load_resource_entry(s32 resource_slot_id, s32 package, s32 entry_index);
void field_release_resource_entry(s32 entry_index);
void field_render_actor_objects(FieldRenderContext* render_context);
void field_set_global_color_scale(s16 red, s16 green, s16 blue);
void field_unpack_resource_package(struct FieldCdBuffer* buf, s32 size, s32 slot_index, s32 palette_row);
void field_finish_party_slot_reload(s32 actor_slot);
/**
 * @brief field_restart_actor_animation(struct FieldActor* actor).
 * @note Declared without parameters: some callers pass a second argument.
 */
void field_restart_actor_animation();
/**
 * @brief field_dispatch_actor_audio_event(struct FieldActorSlot* slot, s32 event_type, s32 event_subtype).
 * @note Declared without parameters: field_effect_update.c passes a fourth argument.
 */
void field_dispatch_actor_audio_event();
u32 field_evaluate_parameter_track_at_time(struct FieldActorSlot* slot, s32 curve_index, s32 frame);
void field_interpolate_palette_track(struct FieldActorSlot* slot, s32 palette_sequence, u8* palette_table, s16* output);
u8* field_begin_actor_animation_forward(struct FieldActor* actor, u8* resource_base);
u8* field_advance_actor_part_animation_frame(struct FieldActor* actor, u8* resource_base);

void field_step_actor_script(struct FieldActor* actor);
void field_route_actor_to_object(struct FieldActor* actor, s32 target_index, s32 restart);
s32 field_resolve_contact_hit(s32 source_index, s32 target_index);
s32 field_resolve_object_hit(s32 source_index, s32 target_index, s32 action);
s32 field_roll_object_evasion(s32 source_index, s32 target_index);
/**
 * @brief field_revive_actor(s32 key, s32 animation, s32 effect, s32 sound).
 * @note Declared without parameters: field_party_state.c calls it with the key only.
 */
s32 field_revive_actor();
s32 field_get_actor_facing(s32 key);
s32 field_spawn_shared_animation_actor(s32 key, s32 resource_index);
s32 field_get_actor_animation(s32 key);
s32 field_register_actor_hit(s32 key, s32 guard);
s32 field_start_actor_defeat_by_key(s32 key, s32 value);
s32 field_schedule_actor_revive(s32 key, s32 animation, s32 effect, s32 sound, s32 delay);
s32 field_read_actor_binding_state(s32 key);
s32 field_face_actor(s32 source_key, s32 target_key);
s32 field_spawn_targeted_animation_actor(s32 key, s32 resource_index, s32 target_keys, s32* unused);
s32 field_start_actor_turn(s32 key);
s32 field_toggle_actor_hidden(s32 key);
s32 field_load_bound_animation(s32 key, s32 resource_id);
s32 field_retire_actor(s32 key, s32 resource_index);
/**
 * @brief field_resolve_collected_hits(s32 object_index).
 * @note Declared without parameters: field_actor_state_updates.c passes a second argument.
 */
void field_resolve_collected_hits();

#define FIELD_ACTION_MIN_FREE_SLOTS 3
s32 field_count_free_actor_slots(s32 object_index);
s32 field_object_has_active_actor_tracks(s32 object_index);
void field_bind_builtin_animations(void);
void field_upload_common_texture(void);
#define FIELD_ACTOR_SLOT_NONE (-1)
s32 field_find_free_actor_slot(s32 binding_index, s32 require_idle);
s32 field_start_builtin_animation(s32 object_index, s32 slot_index, s32 animation_id);
void field_stop_actor_slot(struct FieldActor* actor, struct FieldActorSlot* slot, s32 force);
s32 field_start_streamed_animation(s32 owner, s32 resource_id);
void field_reset_actor_resources(void);
void field_poll_streamed_animations(void);
void field_release_actor_binding(s32 owner);
void field_reset_object_states(void);
void field_reset_object_tints(void);
void field_stop_actor_animations_for_object(struct FieldActor* actor, s32 force);

s32 field_update_actor_landing(struct FieldActor* actor);
void field_play_object_animation(struct FieldActor* actor, s32 animation_id);
void field_update_actor_action_chain(struct FieldActor* actor);
s32 field_update_pending_action(struct FieldActor* actor);
void field_update_instrument_command(struct FieldActor* actor);
void field_update_technique_command(struct FieldActor* actor, s32 sequence_index);
void field_move_actor_step(struct FieldActor* actor, s32 direction_x, s32 vertical_step, s32 direction_z);
void field_update_timed_slide(struct FieldActor* actor, s32 x, s32 z);
void field_follow_leader(struct FieldActor* actor);
void field_update_actor_jump(struct FieldActor* actor, s32 x, s32 y, s32 z);
void field_update_actor_lift(struct FieldActor* actor, s32 rising);
void field_slide_actor(struct FieldActor* actor, s32 dx, s32 dz);
void field_update_timed_walk(struct FieldActor* actor, s32 dx, s32 dz);
s32 field_start_defeat_bound_animation(struct FieldActor* actor);
s32 field_start_defeat_wait_animation(struct FieldActor* actor);
void field_update_defeat_end(struct FieldActor* actor);
void field_restart_idle_animation(struct FieldActor* actor);
void field_update_defeated(struct FieldActor* actor);

s32 field_collect_action_targets(s32 source_index, struct FieldActionSlot* action, s32 group_mode, s32 effect_radius, s32* output);

s32 field_build_group_monster_records(s32 group);
s32 field_group_has_boss(s32 group);
void field_init_monster_record(s32 actor_id, struct FieldStatusRecord* record, struct FieldObjectState* state);

void field_set_battle_group(s32 mode, void* actor_data);
void field_cancel_animation_bindings(void);
void field_update_battle_end(void);

s32 field_load_instrument_bank(s32 bank_index);
void field_upload_resource_22_bank(void);
void field_load_fixed_song(void);
void field_stop_field_song(void);
void field_stop_second_song(void);
void field_play_second_song(void);
void field_fade_song(s32 song, s32 frames, s32 volume);
void field_play_sound_half_pan(s32 sound_id, s32 pan);
void field_play_set_sfx_group0(s32 sfx_index, s32 pan, s32 unused);
void field_play_weapon_sfx(s32 sfx_index, s32 pan, s32 table_index);
void field_release_sfx_group(s32 channel_group);
void field_load_sfx_tables(s32 set_id);
void field_load_weapon_sfx_table(s32 slot, s32 weapon_type);
void field_play_sfx_buffer(struct AkaoHeader* buffer, s32 pan, s32 channel_group);
void field_reset_music_stream(void);
void field_start_music_stream(s32 music_index);
void field_update_music_stream(void);
void field_reset_ring_selections(void);
extern u8 g_field_sound_tables[];
/**
 * @brief field_play_song_section(s32 section_index, s32 param1).
 * @note Declared without parameters: field_script_ops.c calls it with none.
 */
void field_play_song_section();

void field_block_pool_init(void* pool, u32 size);
void* field_block_alloc(void* pool, s32 size, s32 tag);
void field_block_free_tag(void* pool, s32 tag);

extern s32 g_field_card_clock_valid;
s32 field_get_card_clock(struct FieldCardClock* clock);
void field_capture_card_clock(void);

extern s32 g_field_return_to_title_choice;
void field_draw_return_to_title_choices(OT_TYPE* ot, void* prim, s32 scroll_x, s32 scroll_y);

#define FIELD_COLLISION_MARKER_NONE (-1)
s32 field_collision_hit_markers(struct FieldCollisionQuery* query);
s32 field_collision_find_path(struct FieldCollisionQuery* start_query, struct FieldCollisionQuery* goal_query, struct FieldPathPoint* output_path, s32 mode);
void field_collision_dilate_query(struct FieldCollisionQuery* query);
s32 field_collision_resolve_move(struct FieldCollisionMover* mover);
void field_collision_rasterize_groups(u8** allocator_cursor, struct FieldNode* clip);
void* field_header_record_at(s32 index);

#define FIELD_HISTORY_COMMAND_NONE 0xFF
void field_command_history_reset(void);
void field_command_history_record(s32 player, s32 age_sequence);
s32 field_command_history_match(s32 player, s32 unused, s32 peek);
void field_command_history_clear(s32 player);
void field_pair_indicators_reset(void);
u8* field_pair_indicators_get_list(void);
extern u8 g_field_hint_button_map[];

void* field_draw_coordinate_panel(OT_TYPE* ot, u8* prim, s32 x_offset, s32 y_offset);
void field_upload_player_icons(void);
void* field_draw_player_icon(POLY_FT4* handle, OT_TYPE* ordering_table, s32 selector, s32 x, s32 y, s32 flip);
extern s32 g_field_party_has_guest;
extern u8* g_field_coordinate_labels[];

void field_handle_return_to_title_prompt(void);
extern s32 g_field_dialog_screen_mode;
void field_start_timed_panel(s32 index);
void field_clear_actor_texts(void);
s32 field_actor_text_pending(void);
void field_save_retry_snapshot(void);
void field_mark_entrance(void);
void field_update_battle_results_input(void);
void field_close_battle_results(void);
void field_open_battle_results(void);
s32 field_skip_battle_results(void);
void field_open_duel_results(void);
void field_setup_return_to_title_prompt(void);
#define FIELD_ACTOR_TEXT_SAVED_NAME(character, record) (0x80000000 | ((character) << 16) | (record))
void field_start_actor_text(s32 index, s32 text_id);
extern s32 g_field_dialog_item_count;
extern u8 g_field_dialog_item_quantities[];
extern u8* g_field_dialog_item_texts[];
extern u8 g_field_technique_names[];
extern s32 g_field_money_snapshot;
extern s32 g_field_actor_text_count;
extern u32 g_field_experience_snapshot[];
void field_update_timed_panel(struct FieldRenderHalf* render);
void field_update_actor_texts(struct FieldRenderHalf* render);
void field_draw_dialog_windows(struct FieldRenderHalf* render_half);

void field_reset_draw_state(void);

s32 field_spawn_actor_effect(struct FieldActorSlot* actor, s32 part_index, s32 start);
void field_retire_effect(struct FieldMotionRecord* effect, struct FieldObjectPart* part);
void field_advance_actor_effects(struct FieldActorSlot* actor);
extern s32 g_field_action_context;
#define FIELD_ACTION_CONTEXT_ACTION_MASK 0xFF
extern s32 g_field_next_path_group;
extern s32 g_field_spawn_ground_point;
extern s32 g_field_pickup_sound_played;
s32* field_render_effect_frame8(struct FieldMotionRecord* rec, s32* cursor, GPU_OT_ENTRY(s32) * base, u8* item, s32 flag, struct FieldObjectPart* part);
s32* field_render_effect_frame16(struct FieldMotionRecord* rec, s32* cursor, GPU_OT_ENTRY(s32) * base, u8* item, s32 flag, struct FieldObjectPart* part);

s32 field_queue_actor_event(s32 owner_id, u8 event_id, s32 argument);
s32 field_run_actor_event(s32 owner_id, s32 event_id, s32 mode);

void field_set_fade_target_only(s16 red, s16 green, s16 blue, s16 duration);
void field_set_cd_error_fade_target(void);

extern s32 g_field_hide_actor_panels;

void field_write_staged_item(void);
void field_derive_weapon_values(struct FieldItemRecord* record);
void field_derive_armor_values(struct FieldItemRecord* record);

void field_golem_build_group_record(s32 group);

void field_golem_commit_group_edit(s32 command);
void field_golem_select_logic_type(s32 type);
void field_golem_rebuild_current_grid(void);

void field_golem_build_companion(s32 group, struct FieldCharacterRecord* record);

extern s32 g_field_interaction_active;
void field_update_actor_record(s32 actor_id);
void field_end_party_script_control(void);
void field_runtime_update(void);
void field_resolve_talk_window(s32* actor_id, s32* plane, s32* effect, s32* selector);
#define FIELD_TEXT_MACRO_UNLIMITED 0xFF
void field_set_text_macro(s32 slot, u8* text, u8 character_limit);
void field_begin_party_script_control(s32 mode);
/**
 * @brief field_start_interaction(s32 actor_id, s32 script).
 * @note Declared without parameters: field_contact_geometry.c also passes g_field_object_states.
 */
s32 field_start_interaction();
extern SceneState* g_field_scene_state;

void field_open_item_drop_menu(void);
void field_pick_up_item_actor(s32 actor_index);
extern s32 g_field_item_list_count;
extern s32 g_field_item_list_cursor;
extern s32 g_field_primary_repeat_delay;
extern s32 g_field_secondary_repeat_delay;

void field_award_land_artifact(s32 land_index);
void field_reset_lands(void);

void field_run_menu_op(s32 op);

extern s32 g_field_buffered_input;
void field_bind_saved_game_context(void);
void field_open_gosub_screen_sequence(void* screen_sequence);
void field_open_shop_mode_0(s32 shop_options);
void field_open_shop_mode_1(s32 entry_count, struct ShopEntry* entries, struct FieldItemRecord* item_records, s32 shop_options);
void field_run_name_entry(u8* initial_name, u8* active_name, s32 source_mode, s32 history_index, const u8* custom_name);
void field_run_zukan(s32 category);
s32 field_read_controller_buttons(s32 index);
extern s32 g_field_item_drop_enabled;
void field_append_dialog_item(u8* text, u8 quantity);
void field_merge_dialog_items(void);

void field_open_carda(s32 mode);
void field_run_golem(void);
void field_modal_frame_stub(struct FieldRenderHalf* render_half);
void field_open_niki(s32 mode);
void field_open_addhero(s32 mode);

void field_update_pair_indicators(struct FieldRenderHalf* render_half);
extern u8 g_field_pair_indicator_list[];
extern s32 g_field_pair_indicator_counters[];
extern s32 g_field_pair_indicator_count;
extern s32 g_field_pair_indicators_disabled;

extern u8 g_field_element_level_by_land_level[];

void field_roll_menu_slot_effect(s32 group_index, s32 slot_index);
void field_classify_menu_slots(s32 group_index);
void field_apply_pending_region_effects(void);

void field_add_money(s32 recipient, s32 amount);
void field_award_experience(s32 record_index, s32 amount);
void field_apply_character_level_ups(s32 index, s32 notify);
void field_apply_pet_level_ups(s32 slot);
s32 field_try_character_level_up(s32 index, s32 notify);
s32 field_add_stat_increase(s32 value, u32 increase, s32 flags);

void field_sort_keyed_list(struct FieldKeyedList* list);

struct FieldActorRecord* field_alloc_actor_record(s32 id);
void field_stop_actor_script(s32 actor_id, s32 flags);
void field_restore_actor_capacity_fraction(s32 record_id, s32 fraction_256);
void field_grant_actor_pickup(s32 recipient, s32 owner_id);
/**
 * @brief field_find_actor_record(s32 id).
 * @note Declared without parameters: field_interaction_start.c calls it with no argument.
 */
struct FieldActorRecord* field_find_actor_record();
/**
 * @brief field_find_actor_record_or_default(s32 id).
 * @note Declared without parameters: field_interaction_start.c also passes g_field_runtime.
 */
struct FieldActorRecord* field_find_actor_record_or_default();
void field_stop_non_script_actors(void);
void field_actor_loop_stub(void);

s32 field_distance_xz(Vec3i* first, Vec3i* second);
s32 field_is_actor_near_stored_position(s32 actor_id, s32 half_width, s32 half_depth);

void* field_find_resource(s32 resource_id);
void* field_copy_words(const void* src, void* dest, s32 size);

void field_create_item_from_gosub(s32 kind);
void field_create_equipment_item(struct FieldItemRecord* record, s32 category, s32 item_type, s32 material);
void field_temper_item(struct FieldItemRecord* record, s32 secondary_item);

void field_refresh_party_member(s32 index);
s32 field_get_equipped_stat(struct FieldCharacterRecord* character, u32 stat_index);
extern s8 g_field_stat_modifier_values[];

void field_unlock_land_artifact(s32 land_index);
s32 field_get_land_state(s32 land_index);
s32 field_get_land_distance(s32 land_index);
void field_generate_item_key(u32 seed, struct FieldItemKey* out);
s32 field_receive_money(s32 amount);
s32 field_spend_money(u32 amount);
s32 field_get_item_value(struct FieldItemRecord* item);
void field_cache_inventory_values(void);
s32 field_award_land_artifacts(s32 first_land, s32 second_land, s32* awarded);

void field_reset_battle_entry(void);
s32 field_begin_battle_entry(void);
void field_update_battle_entry(void);
s32 field_queue_battle_entry_change(s32 actor_key, s32 animation, s32 builtin_animation, s32 sound);
s32 field_party_reload_pending(void);
void field_install_party_reload(s32 alternate_layout, s32 slot);
void field_request_party_reload(s32 weapon_set);
s32 field_party_reload_reading(void);

u8* field_get_party_event_script(s32 page, u16 entry);
u8* field_get_party_private_script(s32 page, u16 entry);
struct FieldActionDescriptor* field_get_party_action(s32 page, u16 index);
void field_unlock_encyclopedia_entry(u32 entry);
s32 field_add_template_item(s32 index);
void field_discard_item(s32 index);

s32 field_roll_defeat_drop(struct FieldStatusRecord* record);
void field_grant_reward(s32 recipient, s32 owner, u32 kind);

void field_open_ring_menu(s32 position_mode, s32 menu_id, u16 excluded_mask, s32 cancel_index);
s32 field_get_ring_result(void);
s32 field_get_ring_cursor_entry(void);
s32 field_update_ring_menu(struct FieldRenderHalf* render_half);
#define FIELD_RES_PARTNER_ACTIONS 0xA37
#define FIELD_RES_COMPANION_ACTIONS 0xA9B
void field_load_party_script_page(s32 party_slot, s32 resource_id);
void field_upload_golem_palettes(void);
extern s32 g_field_ring_menu_state;
extern u8 g_field_ring_saved_selections[];

s32 field_add_stored_companion(s32 template_index);
s32 field_release_stored_companion(void);
s32 field_get_stored_companion_status(s32 index);
void field_rename_stored_companion(s32 index);

extern u8* g_field_cd_buffer;
extern u8* g_field_actor_heap;
extern s32 g_field_scene_mode_bit;

extern s32 g_field_duel_mode;
void field_script_op_00(void);
void field_script_run(struct FieldScriptState* context);

extern s32 g_field_scene_contact_latched;
extern u16* g_field_event_scripts;
extern u32* g_field_scene_record_table;
extern u16* g_field_actor_scripts;
extern s32 g_field_loaded_actor_count;
extern struct FieldRenderHalf* g_field_render_context;
extern s32 g_field_preserve_entry_music;
extern u8* g_field_scene_portraits;
extern u8* g_field_scene_strings;
extern s32 g_field_last_actor_multi_image;

void field_draw_scene_objects(u8** cursor, OT_TYPE* ot, s32 update_mode);
void field_size_work_buffer(void);

void field_set_color_scale(s32 object_index, s32 red, s32 green, s32 blue);
void field_begin_scene_fade_in(void);

enum
{
    FIELD_LIST_TILE_ANIMS = 0,
    FIELD_LIST_PALETTE_ANIMS = 1,
    FIELD_LIST_TINT_ANIMS = 2,
    FIELD_LIST_SEQUENCES = 3
};

enum
{
    FIELD_ANIM_OP_START = 0,
    FIELD_ANIM_OP_STOP = 1,
    FIELD_ANIM_OP_RESTART = 2,
    FIELD_ANIM_OP_SEEK = 3,
    FIELD_ANIM_OP_FINISH_LOOP = 4
};

#define FIELD_WHOLE_OBJECT (-1)
#define FIELD_KEYFRAME_NONE (-1)

void field_apply_pixel_lookup(u16* pixels, s32 pixel_count, s32 table_index, void* unused);
void field_control_animation(s32 list_kind, s32 index, s32 keyframe, s32 op);
void field_update_scene_fade(void);
void field_set_object_visible(s32 obj_index, s32 part_index, s32 visible);
void field_get_object_position(s32 obj_index, s32 part_index, FieldPos* out);
void field_play_effect_animation(s32 index, s32 forward);
void field_control_sequence(s32 index, s32 op);
s32 field_get_animation_state(s32 list_kind, s32 index);
void field_begin_scene_fade_out(void);
s32 field_is_scene_fading(void);
void field_set_node_enabled(s32 index, s32 enabled);
void field_set_pixel_lookup(s32 selector);

void field_load_map(u16 map_id);
void field_init_ctx(struct FieldRenderHalf* buffers, u16 object_index);

s32 field_script_calc(s32 op, s32 left, s32 right);

void field_script_command(s32 command, void* params);

s32 field_select_distance_bucket(s32 actor_id);

void field_run_item_script(s32 offset);
void field_run_slot_scripts(void);
s32 field_replace_slot_value(s32 cost, s32 value, s32 replacement);
void field_finish_staged_item(void);
void field_apply_pending_levels(void);
void field_raise_staged_level(s32 index);
void field_lower_staged_level(s32 index);
s32 field_can_pay_staged_cost(s32 cost);

void field_count_revived_record(s32 key);
void field_rebuild_equipment_status_flags(struct FieldStatusRecord* record);
void field_tick_battle_status(void);
u32 field_count_status_slots(struct FieldStatusRecord* record, s32 status);

void field_reset_item_menu(void);
void field_update_item_menu(struct FieldRenderHalf* render_half);

void field_open_text_window(s32 window_slot, s32 layout_index, s32 unused, s32 portrait_selector);
void field_set_text_window_string(s32 window_slot, s32 string_index, s32 options);
void field_open_text_window_with_string(s32 string_index, s32 window_slot, s32 layout_index, s32 unused, s32 portrait_selector);
void field_close_text_window(s32 window_slot);
void field_show_timed_text(s32 string_index);

extern s32 g_field_song_volume;

#define FIELD_INTERACTION_SCRIPT 0x8000

#endif
