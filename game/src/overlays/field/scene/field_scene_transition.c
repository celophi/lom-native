#include "common.h"
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include "lom_native_input.h"
#include "lom_native_sound.h"
#include "lom_native_time.h"
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"
#include "lom_native_mods.h"
#include "lom_native_japanese_balance.h"

#include "../internal/field_effect_types.h"
#include "../internal/field_actor_palette.h"
#include "../internal/field_actor_routes.h"
#include "../internal/field_actor_runtime.h"
#include "../internal/field_actor_tables.h"
#include "../internal/field_calls.h"
#include "../internal/field_effect_render_state.h"
#include "../internal/field_state_ops.h"
#include "common/tim.h"
#include "main/audio/akao_cmd.h"
#include "main/audio/game_audio.h"
#include "main/cd_resources.h"
#include "main/cdrom.h"
#include "main/controller_internal.h"
#include "main/field_entry.h"
#include "main/main.h"
#include "main/scene_state.h"
#include "overlays/field/field_actor_records.h"
#include "overlays/field/field_interaction_start.h"
#include "overlays/field/field_portrait.h"
#include "overlays/field/field_scene_transition.h"
#include "overlays/field/field_text.h"
#if defined(LOM_NATIVE_POINTERS)
void lom_native_field_resource_overlap_pull(void);
void lom_native_field_resource_overlap_push(s32 entry_end);
#endif

#define FIELD_SCENE_MODE_FLAG 0x8000
#define FIELD_SCENE_FILE ((u8*)LOAD_BUFFER_AT(0x10000))
#define FIELD_SCENE_FILE_ROOM 0x70000
#define FIELD_SCENE_MAP_ID_MASK 0x7FFF
#define FIELD_SCENE_PARTS_TIMER_SHIFT 15
#define FIELD_MUSIC_KEEP -1
#define FIELD_MUSIC_FIXED -2
#define FIELD_MUSIC_FADE_FRAMES 60
#define FIELD_ENTRY_MUSIC_TIMER 15
#define FIELD_ACTOR_IMAGE_COUNT 6
#define FIELD_ACTOR_IMAGE_END 0xFF
#define FIELD_CHEST_RESOURCE 8
#define FIELD_CHEST_TEXTURE_SLOT 7
#define FIELD_PARTY_PALETTE_COUNT 6
#define FIELD_FIRST_IMAGE_SLOT 2
#define FIELD_IMAGE_SLOT_SKIP_FIRST 6
#define FIELD_IMAGE_SLOT_SKIP_COUNT 4
#define FIELD_IMAGE_SLOT_SECOND_ROW 10
#define FIELD_IMAGE_ROW0_X 832
#define FIELD_IMAGE_ROW1_X 960
#define FIELD_IMAGE_ROW1_Y 256
#define FIELD_SCENE_NO_LAYOUT_GOLEM_COLORS 0x13D
#define FIELD_DEFAULT_SPAWN_X 160
#define FIELD_DEFAULT_SPAWN_Z 224
#define FIELD_PARTY_START_SPACING 40
#define FIELD_DIRECTION_ANIMATION_COUNT 5
#define FIELD_TRANSITION_TILE_SIZE 32
#define FIELD_TRANSITION_TILE_X 288
#define FIELD_TRANSITION_TILE_Y0 180
#define FIELD_TRANSITION_TILE_Y1 412
#define FIELD_TRANSITION_TILE_PIXELS 0x800
#define FIELD_FADE_WHITE 0x1FF
#define FIELD_PIXEL_COLOR_MASK 0x7FFF
#define FIELD_PIXEL_TRANSPARENT 0
#define FIELD_PIXEL_WHITE 0xFFFF

#define FIELD_SPAWN_FACING(spawn) ((spawn)->h.facing & 0xF)

#define FIELD_MAP_X_LIMIT(bounds) ((bounds)->width << FIELD_POSITION_SHIFT)
#define FIELD_MAP_Z_LIMIT(bounds) ((s16)(bounds)->depth << (FIELD_POSITION_SHIFT + 1))

#define OFFSET_FIRST_PTR(type, offset, base) ((type*)((offset) + (uintptr_t)(base)))

enum
{
    FIELD_SCENE_LAYOUT = 0x00,
    FIELD_SCENE_EVENT_SCRIPTS = 0x04,
    FIELD_SCENE_STRINGS = 0x08,
    FIELD_SCENE_ACTOR_SCRIPTS = 0x0C,
    FIELD_SCENE_RECORDS = 0x10,
    FIELD_SCENE_ACTORS = 0x14,
    FIELD_SCENE_GEOMETRY = 0x18,
    FIELD_SCENE_IMAGES = 0x1C,
    FIELD_SCENE_PORTRAITS = 0x20,
    FIELD_SCENE_GROUP_BOUNDS = 0x24
};

#define FIELD_SCENE_SECTION(file, entry) ((file) + *(s32*)((file) + (entry)))

typedef struct
{
    u8 unused_00;
    u8 flags;
    u8 images[FIELD_ACTOR_IMAGE_COUNT];
    u16 palette;
    u16 bound_animation_flags;
    s32 action_count;
    FieldActionSlot actions[1];
} FieldSceneActorDescription;

typedef struct
{
    u32 size;
    RECT rect;
    u32 pixels[1];
} FieldTimBlock;

typedef struct
{
    u32 magic;
    u32 flags;
    FieldTimBlock palette;
} FieldTimFile;

typedef union
{
    FieldTimFile file;
    FieldTimBlock block;
} FieldTimData;

typedef union
{
    struct
    {
        s16 x, y, z;
        u8 facing;
        u8 flags;
    } h;

    struct
    {
        u32 xy;
        u32 z : 16;
        u32 facing : 4;
        u32 unused_20 : 12;
    } bits;
} FieldSceneSpawn;

typedef struct
{
    s32 count;
    FieldLayoutRecord records[1];
} FieldSceneLayout;

/** @brief field_modal_runtime.c; called here without a prototype, so the arguments are passed unconverted. */
void field_store_entry_settings();

extern u8 g_field_chest_geometry[0x3C];
extern u16 g_field_party_palettes[FIELD_PARTY_PALETTE_COUNT];
extern u16 g_field_transition_tiles[FIELD_TRANSITION_TILE_PIXELS];
extern u16 g_field_transition_tiles_alt[FIELD_TRANSITION_TILE_PIXELS / 2];
extern s32 g_field_transition_tile_bank;
extern s32 g_field_pending_spawn_id;
extern s32 g_field_pending_music_id;
extern s32 g_field_pending_secondary_music_id;
extern s32 g_field_pending_scene_id;
extern s32 g_field_pending_object_id;
extern s32 g_field_pending_sound_bank_id;

extern s32 g_field_scene_data_size;
extern u8 g_field_scene_data_buffer[];
extern s32 g_field_previous_song_volume;
extern s32 g_field_party_palette_index;
extern u32 g_field_scene_portrait_count;
static void field_setup_chest_resource(void);
void field_upload_actor_image(FieldTimData* image, s32 image_slot, s32 actor_index, s32 upload_palette);
void field_copy_scene_geometry(s32* src, s32* end);
void field_load_scene_actors(FieldSceneLayout* layout);
static void field_refresh_actor_collisions(void);
static void field_upload_transition_tiles(void);
static void field_prepare_transition_tiles(void);

/**
 * @brief Queue a seek to the scene selected by the low fifteen selector bits.
 * @param scene_selector Scene resource selector, optionally carrying the high-bit mode flag.
 */
void field_seek_scene_resource(s32 scene_selector)
{
    cdrom_queue_seek((scene_selector & FIELD_SCENE_ID_MASK) + CD_RES_FIELD_SCENE_BASE);
}

/**
 * @brief Record the parameters for a pending scene change.
 * @param scene_id Scene id (with the high-bit mode flag preserved).
 * @param object_id Field object selected for the render context.
 * @param spawn_id Spawn id.
 * @param music_id Primary music resource; -1 retains music, -2 loads the fixed sequence.
 * @param sound_bank_id Sound-bank resource; -1 clears its header, -2 retains it.
 * @param secondary_music_id Secondary music resource, or -1 to retain it.
 */
void field_set_scene_parameters(s32 scene_id, s32 object_id, u32 spawn_id, s32 music_id, s32 sound_bank_id, s32 secondary_music_id)
{
    g_field_pending_scene_id = scene_id;
    g_field_pending_object_id = object_id;
    g_field_pending_spawn_id = spawn_id;
    g_field_pending_music_id = music_id;
    g_field_scene_request_pending = 1;
    g_field_pending_sound_bank_id = sound_bank_id;
    g_field_pending_secondary_music_id = secondary_music_id;
}

void field_preserve_scene_sections(u8* scene_file)
{
    u8* strings_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_STRINGS);
    u8* event_scripts_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_EVENT_SCRIPTS);
    u8* actor_scripts_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_ACTOR_SCRIPTS);
    u8* records_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_RECORDS);
    u8* actors_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_ACTORS);
    u8* portraits_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_PORTRAITS);
    u8* data_output;
    u8* section_source;
    s32 i, image_count;
    u32 portrait_count;
    data_output = g_field_scene_data_buffer;
    g_field_scene_strings = (g_field_scene_data_buffer);
    section_source = strings_section;
    while (section_source != actor_scripts_section)
    {
        *data_output++ = *section_source++;
    }
    g_field_event_scripts = ((u16*)ALIGN4(data_output));
    section_source = event_scripts_section;
    while (section_source != strings_section)
    {
        *data_output++ = *section_source++;
    }
    g_field_actor_scripts = ((u16*)ALIGN4(data_output));
    section_source = actor_scripts_section;
    while (section_source != records_section)
    {
        *data_output++ = *section_source++;
    }
    g_field_scene_record_table = ((u32*)ALIGN4(data_output));
    section_source = records_section;
    while (section_source != actors_section)
    {
        *data_output++ = *section_source++;
    }
    section_source = portraits_section + 4;
    i = 0;
    portrait_count = *(u32*)portraits_section;
    g_field_scene_portraits = ((u8*)ALIGN4(data_output));
    g_field_scene_portrait_count = portrait_count;
    while ((u32)i < g_field_scene_portrait_count)
    {
        image_count = 0;
        do
        {
            image_count++;
            *data_output++ = *section_source++;
        } while (image_count < FIELD_PORTRAIT_BYTES);
        i++;
    }
    g_field_scene_data_size = data_output - g_field_scene_data_buffer;
}

void field_place_party_at_spawn(s32 spawn_id)
{
    FieldSceneSpawn default_spawn;
    FieldSceneSpawn* spawn;
    FieldActor* actor;
    FieldDirectionOffset* direction_offsets;
    s32* direction_modes;
    s32* direction_entry;
    s32 offset[3];
    s32 i, direction_flag;
    s8 animation;
    spawn = field_header_record_at((u16)spawn_id);
    if (spawn == NULL)
    {
        default_spawn.h.z = FIELD_DEFAULT_SPAWN_Z;
        default_spawn.h.x = FIELD_DEFAULT_SPAWN_X;
        default_spawn.h.y = 0;
        default_spawn.bits.facing = 0;
        spawn = &default_spawn;
    }
    actor = g_field_actors;
    i = 0;
    direction_offsets = g_field_direction_offsets;
    direction_modes = g_field_direction_animation_modes;
    for (; i < FIELD_PARTY_COUNT; i++, actor++)
    {
        if (actor->presence != FIELD_ACTOR_UNUSED)
        {
            actor->x = spawn->h.x << FIELD_POSITION_SHIFT;
            actor->y = spawn->h.y << FIELD_POSITION_SHIFT;
            actor->z = spawn->h.z << FIELD_POSITION_SHIFT;
            offset[0] = direction_offsets[FIELD_SPAWN_FACING(spawn)].dx * i;
            offset[1] = 0;
            offset[2] = direction_offsets[FIELD_SPAWN_FACING(spawn)].dz * i;
            field_move_actor_position(actor, offset);
            actor->direction = (FIELD_SPAWN_FACING(spawn)) << 5;
            direction_entry = OFFSET_FIRST_PTR(s32, (FIELD_SPAWN_FACING(spawn)) * sizeof(s32), direction_modes);
            animation = (*direction_entry & ~FIELD_ANIMATION_FACING) % FIELD_DIRECTION_ANIMATION_COUNT;
            direction_flag = *(u8*)direction_entry & FIELD_ANIMATION_FACING;
            animation |= direction_flag;
            actor->animation = animation;
            actor->animation_frame = 0;
            actor->animation_active = 1;
            field_restart_actor_animation(actor);
        }
    }
    field_refresh_actor_collisions();
    field_reset_leader_position_history();
    field_camera_track_party();
    g_field_camera_follow_x = g_field_camera_target_x;
    g_field_camera_follow_z = g_field_camera_target_z;
}

/**
 * @brief Load the scene FIELD has been asked for, when one is pending: its map,
 *        layout, actors, sounds and the party's place in it.
 * @note Native: once the scene is built, raises the mods' scene_enter hook with
 *       the scene's number.
 */
void field_update_scene(void)
{
    FieldSceneSpawn default_spawn;
    RECT rect;
    s32 offset[3];
    u8* actor_description_base;
    u8* image_base;
    u8* geometry_base;
    FieldSceneLayout* layout;
    SceneState* scene_state;
    s32 actor_end;
    s32 object_id;
    s32 spawn_id;
    s32 sound_bank_id;
    s32 action_offset;
    u8* scene_file;
    ControllerPortState* controller_ports;
    FieldSceneSpawn* spawn;
    s32* group_bounds_list;
    u32* group_bounds_source;
    u32* group_bounds_output;
    FieldActor* actor;
    u8* scene_cursor;
    s32 direction_flag;
    s32 scene_id;
    s32 previous_mode;
    s32 saved_scene_id;
    s32 action_index;
    s32 resource_offset;
    s32 image_index;
    s32 image_count;
    s32 i;
    s32 next_i;
    u8* resource_base;
    u8* resource_table;
    u8* action_base;
    FieldDirectionOffset* direction_offsets;
    s32* direction_modes;
    s8 animation;
    s32 top_image_slot;
    FieldActionSlot* action_ids;
    u8* portraits_section;
    u8* event_scripts_section;
    u32 portrait_count;
    u8* strings_section;
    u8* actor_scripts_section;
    u8* records_section;
    u8* actors_section;
    u8* section_source;
    u8* data_output;
    u8* image_cursor;
    FieldResourceEntry* resource_fields;
    FieldActionSlot* action;
    FieldResourceEntry* resource;
    FieldResourceEntry* inherited_resource;
    FieldSceneActorDescription* description;
    FieldActionSlot* action_fields;
    s32* geometry_offsets;
    s32* direction_entry;
    FieldResourceEntry* previous_resource;

    scene_state = SCENE_STATE;
    controller_ports = lom_input_ports();

    if (g_field_scene_request_pending != 0)
    {
        controller_ports[1].small_motor_command = 0;
        controller_ports[0].small_motor_command = 0;
        controller_ports[1].actuator_control.fields.large_motor_command = 0;
        controller_ports[0].actuator_control.fields.large_motor_command = 0;

        if (g_field_audio_timer != 0)
        {
            fade_out_current_song();

            if (g_field_pending_music_id == FIELD_MUSIC_KEEP)
            {
                field_play_song();
                lom_sound_fade_song_volume(0, 1, g_field_song_volume);
            }

            g_field_audio_timer = 0;
        }

        field_prepare_transition_tiles();
        field_upload_transition_tiles();
        scene_id = g_field_pending_scene_id;
        g_field_scene_request_pending = 0;
        object_id = g_field_pending_object_id;
        spawn_id = g_field_pending_spawn_id;
        sound_bank_id = g_field_pending_sound_bank_id;
        lom_graphics_wait(0);
        previous_mode = g_field_scene_mode_bit;
        g_field_scene_mode_bit = scene_id & FIELD_SCENE_MODE_FLAG;
        scene_id = scene_id & FIELD_SCENE_ID_MASK;

        if (g_field_pending_music_id != FIELD_MUSIC_KEEP)
        {
            if (g_field_preserve_entry_music == 0)
            {
                lom_sound_fade_song_volume(0, FIELD_MUSIC_FADE_FRAMES, 0);
            }
        }

        scene_file = FIELD_SCENE_FILE;
        cdrom_queue_read((u16)(scene_id + CD_RES_FIELD_SCENE_BASE), scene_file);
        i = 0;
        cdrom_wait_queue_empty();
#if !defined(VERSION_JP)
        lom_japanese_balance_scene(scene_id, scene_file, FIELD_SCENE_FILE_ROOM);
#endif
        field_upload_transition_tiles();
        g_field_entry_scene.word = scene_id;
        g_field_active_group = 0;
        g_field_duel_mode = 0;
        field_reset_battle_entry();
        field_clear_actor_texts();
        lom_sound_release_all_effects();
        field_initialize_actor_system();
        field_upload_transition_tiles();

        for (; i < FIELD_PARTY_COUNT; i++)
        {
            g_field_actors[i].x = (FIELD_DEFAULT_SPAWN_X + i * FIELD_PARTY_START_SPACING) << FIELD_POSITION_SHIFT;
            g_field_actors[i].y = 0;
            g_field_actors[i].z = FIELD_DEFAULT_SPAWN_Z << FIELD_POSITION_SHIFT;
        }

        if (previous_mode != g_field_scene_mode_bit)
        {
            field_load_instrument_bank(g_field_scene_mode_bit == 0);
        }

        field_upload_transition_tiles();
        group_bounds_output = (u32*)g_field_group_bounds;
        g_field_dialog_item_count = 0;
        scene_cursor = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_ACTORS);
        group_bounds_list = (s32*)(FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_GROUP_BOUNDS));
        group_bounds_source = (u32*)(group_bounds_list + 1);
        i = group_bounds_list[0];
        actor_description_base = scene_cursor;
        actors_section = scene_cursor;
        g_field_group_bounds_count = i;
        geometry_base = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_GEOMETRY);
        image_base = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_IMAGES);
        layout = (FieldSceneLayout*)(FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_LAYOUT));
        strings_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_STRINGS);
        event_scripts_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_EVENT_SCRIPTS);
        actor_scripts_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_ACTOR_SCRIPTS);
        records_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_RECORDS);
        portraits_section = FIELD_SCENE_SECTION(scene_file, FIELD_SCENE_PORTRAITS);

        while (i != 0)
        {
            *group_bounds_output++ = *group_bounds_source++;
            i--;
        }
        field_preserve_scene_sections(scene_file);
        g_field_previous_song_volume = g_field_song_volume;
        g_field_song_volume = *(u16*)scene_cursor;
        g_field_party_palette_index = *(u16*)(scene_cursor + 2);
        scene_cursor += 4;
        field_set_party_palettes();
        i = FIELD_PARTY_COUNT;
        field_initialize_actor_parts(*(u16*)scene_cursor >> FIELD_SCENE_PARTS_TIMER_SHIFT);
        g_field_hide_actor_panels = 0;
        field_upload_transition_tiles();
        scene_state->map_id = *(u16*)scene_cursor & FIELD_SCENE_MAP_ID_MASK;
        scene_cursor += 2;
        actor = &g_field_actors[FIELD_PARTY_COUNT];
        actor_end = *(u16*)scene_cursor + FIELD_PARTY_COUNT;
        scene_cursor += 2;

        for (; i < FIELD_ACTOR_COUNT; i++, actor++)
        {
            actor->presence = FIELD_ACTOR_UNUSED;
        }

        top_image_slot = FIELD_FIRST_IMAGE_SLOT;
        g_field_last_actor_multi_image = 0;
        action_base = (u8*)g_field_resource_actions;
        field_setup_chest_resource();
        i = 0;
#if defined(LOM_NATIVE_POINTERS)
        lom_native_field_resource_overlap_pull();
#endif

        if ((actor_end - FIELD_PARTY_COUNT) > 0)
        {
            action_offset = FIELD_PARTY_COUNT * sizeof(FieldActionRow);
            geometry_offsets = (s32*)geometry_base;
            resource_offset = FIELD_PARTY_COUNT * sizeof(FieldResourceEntry);

            do
            {
                image_count = 0;
                description = (FieldSceneActorDescription*)(actor_description_base + *(s32*)scene_cursor);
                image_cursor = description->images;
                resource_table = (u8*)g_field_resource_entries;

                while (*image_cursor != FIELD_ACTOR_IMAGE_END)
                {
                    image_count++;
                    image_cursor++;

                    if (image_count == FIELD_ACTOR_IMAGE_COUNT)
                    {
                        break;
                    }
                }

                top_image_slot += image_count;

                if (top_image_slot >= FIELD_IMAGE_SLOT_SKIP_FIRST && top_image_slot < FIELD_IMAGE_SLOT_SKIP_FIRST + FIELD_IMAGE_SLOT_SKIP_COUNT)
                {
                    top_image_slot += FIELD_IMAGE_SLOT_SKIP_COUNT;
                }

                if (image_count != 0)
                {
                    resource_base = resource_table;
                    resource_fields = OFFSET_FIRST_PTR(FieldResourceEntry, resource_offset, resource_base);
                    resource_fields->attribute.palette = description->palette;
                    resource_fields->slot_index = top_image_slot;

                    if (g_field_last_actor_multi_image != 0)
                    {
                        resource_fields->multi_image = 0;
                    }
                    else
                    {
                        resource_fields->multi_image = image_count != 1;
                    }

                    g_field_last_actor_multi_image = image_count != 1;
                    resource_base = resource_table;
                    resource = OFFSET_FIRST_PTR(FieldResourceEntry, resource_offset, resource_base);
                    resource->start = g_field_resource_cursor;
                    resource->flags = (resource->flags & ~FIELD_RESOURCE_HAS_ACTIONS) | (description->flags >> 7);
                    field_copy_scene_geometry((s32*)(geometry_base + geometry_offsets[0]), (s32*)(geometry_base + geometry_offsets[1]));
                    resource->end = g_field_resource_cursor;
                    resource->flags |= FIELD_RESOURCE_LOADED;
                    action_ids = description->actions;
                    resource->bound_animation_flags = description->bound_animation_flags;
                    image_index = description->action_count;
                    action_index = 0;

                    if (image_index > 0)
                    {
                        action_fields = description->actions;

                        do
                        {
                            action = OFFSET_FIRST_PTR(FieldActionSlot, action_offset + action_index * sizeof(FieldActionSlot), action_base);
                            action->flags.instrument = action_fields->flags.instrument;
                            action->command = action_ids->command;
                            action->flags.target_filter = action_fields->flags.target_filter;
                            action_index++;
                            action->animation = action_fields->animation;
                            action_ids++;
                            action->parameter = action_fields->parameter;
                            action->flags.target_group = action_fields->flags.target_group;
                            action_fields++;
                        } while (action_index < image_index);
                    }

                    image_index = 0;
                }
                else
                {
                    rect.y = i + FIELD_RESOURCE_CLUT_VRAM_Y + FIELD_PARTY_COUNT - 1;
                    rect.w = GPU_CLUT_8BIT_COLORS;
                    rect.x = 0;
                    rect.h = 1;
                    lom_graphics_copy(&rect, 0, i + FIELD_RESOURCE_CLUT_VRAM_Y + FIELD_PARTY_COUNT);
                    previous_resource = (FieldResourceEntry*)((u8*)g_field_resource_entries + (i + FIELD_PARTY_COUNT - 1) * sizeof(FieldResourceEntry));
                    resource_base = (u8*)g_field_resource_entries;
                    inherited_resource = OFFSET_FIRST_PTR(FieldResourceEntry, resource_offset, resource_base);
                    inherited_resource->attribute.palette = previous_resource->attribute.palette;
                    inherited_resource->slot_index = previous_resource->slot_index;
                    inherited_resource->multi_image = 0;
                    inherited_resource->start = g_field_resource_cursor;
                    inherited_resource->flags = (inherited_resource->flags & ~FIELD_RESOURCE_HAS_ACTIONS) | (description->flags >> 7);
                    field_copy_scene_geometry((s32*)(geometry_base + geometry_offsets[0]), (s32*)(geometry_base + geometry_offsets[1]));
                    inherited_resource->end = g_field_resource_cursor;
                    inherited_resource->flags |= FIELD_RESOURCE_LOADED;
                    action_ids = description->actions;
                    inherited_resource->bound_animation_flags = description->bound_animation_flags;
                    image_index = description->action_count;
                    action_index = 0;

                    action_fields = description->actions;

                    while (action_index < image_index)
                    {
                        action = OFFSET_FIRST_PTR(FieldActionSlot, action_offset + action_index * sizeof(FieldActionSlot), action_base);
                        action->flags.instrument = action_fields->flags.instrument;
                        action->command = action_ids->command;
                        action->flags.target_filter = action_fields->flags.target_filter;
                        action_index++;
                        action->animation = action_fields->animation;
                        action_ids++;
                        action->parameter = action_fields->parameter;
                        action->flags.target_group = action_fields->flags.target_group;
                        action_fields++;
                    }

                    lom_graphics_wait(0);
                    image_index = 0;
                }

                image_cursor = description->images;

                while (image_count != 0)
                {
                    field_upload_actor_image((FieldTimData*)(image_base + *(s32*)(image_base + (*image_cursor << 2))), top_image_slot - image_index,
                                             i + FIELD_PARTY_COUNT, image_index == 0);
                    image_cursor++;
                    image_count--;
                    image_index++;
                }

                next_i = i + 1;
                geometry_offsets++;
                resource_offset += sizeof(FieldResourceEntry);
                i = next_i;
                action_offset += sizeof(FieldActionRow);
                scene_cursor += 4;
            } while (i < (actor_end - FIELD_PARTY_COUNT));
        }
#if defined(LOM_NATIVE_POINTERS)
        lom_native_field_resource_overlap_push(actor_end);
#endif

        field_upload_transition_tiles();
        field_reset_object_tints();
        field_load_scene_actors(layout);

        if (g_field_pending_music_id != FIELD_MUSIC_KEEP)
        {
            field_capture_card_clock();

            if (g_field_preserve_entry_music == 0)
            {
                field_stop_song();
            }

            if (g_field_pending_music_id != FIELD_MUSIC_FIXED)
            {
                field_load_song(g_field_pending_music_id, 0);

                if ((g_field_preserve_entry_music == 0) && (g_field_pending_music_id != FIELD_MUSIC_FIXED))
                {
                    field_play_song();
                }
            }
            else
            {
                field_load_fixed_song();
            }

            g_field_music_id = g_field_pending_music_id;
        }
        else
        {
            lom_sound_fade_song_volume(0, FIELD_MUSIC_FADE_FRAMES, g_field_song_volume);
        }

        if (g_field_pending_secondary_music_id != FIELD_MUSIC_KEEP)
        {
            field_load_song(g_field_pending_secondary_music_id, 1);
            g_field_secondary_music_id = g_field_pending_secondary_music_id;
        }

        field_upload_transition_tiles();

        if (g_field_preserve_entry_music != 0)
        {
            g_field_audio_timer = FIELD_ENTRY_MUSIC_TIMER;
        }
        else
        {
            g_field_audio_timer = 0;
        }

        g_field_preserve_entry_music = 0;

        if (g_field_scene_mode_bit != 0)
        {
            field_upload_resource_22_bank();
        }

        field_upload_transition_tiles();
        lom_graphics_wait(0);
        lom_time_wait(0);
        field_upload_transition_tiles();
        field_load_map(scene_state->map_id);
        field_upload_transition_tiles();
        field_init_ctx(g_field_render_context, object_id);
        field_upload_transition_tiles();
        field_text_reset_windows();
        field_camera_reset();

        if (g_field_scene_mode_bit != 0)
        {
            field_battle_setup(0);
        }
        field_place_party_at_spawn(spawn_id);
        field_load_sfx_tables(sound_bank_id);
        g_field_sound_bank_id = sound_bank_id;
        field_upload_transition_tiles();
        g_field_scene_contact_latched = 0;
        field_initialize_actor_slots();
        field_clear_actor_slots();
        field_reset_draw_state();
        field_reset_actor_resources();
        field_command_history_reset();
        field_clear_fade_prims();
        saved_scene_id = g_field_entry_scene.word;

        if (g_field_scene_mode_bit != 0)
        {
            saved_scene_id += FIELD_SCENE_MODE_FLAG;
        }

        field_store_entry_settings(saved_scene_id, object_id, g_field_music_id, spawn_id, g_field_sound_bank_id, g_field_secondary_music_id);
        field_upload_golem_palettes();
        field_refresh_party_routes();
        field_pair_indicators_reset();
        field_reset_item_menu();
        field_upload_transition_tiles();
        field_save_retry_snapshot();
        lom_native_mods_hook(LOM_MOD_HOOK_SCENE_ENTER, scene_id);
    }
}

/**
 * @brief Set up the shared chest sprites for the scene.
 *
 * The common FIELD texture supplies the closed and open images. Scene scripts
 * select the image and handle the reward and saved collection flag.
 */
static void field_setup_chest_resource(void)
{
    g_field_resource_entries[FIELD_CHEST_RESOURCE].slot_index = FIELD_CHEST_TEXTURE_SLOT;
    g_field_resource_entries[FIELD_CHEST_RESOURCE].start = g_field_chest_geometry;
    g_field_resource_entries[FIELD_CHEST_RESOURCE].end = g_field_chest_geometry + sizeof(g_field_chest_geometry);
    g_field_resource_entries[FIELD_CHEST_RESOURCE].flags &= ~FIELD_RESOURCE_HAS_ACTIONS;
    g_field_resource_entries[FIELD_CHEST_RESOURCE].attribute.palette = 0;
    g_field_resource_entries[FIELD_CHEST_RESOURCE].multi_image = 0;
    g_field_resource_entries[FIELD_CHEST_RESOURCE].bound_animation_flags = 0;
    g_field_resource_entries[FIELD_CHEST_RESOURCE].flags |= FIELD_RESOURCE_LOADED;
}

/**
 * @brief Upload an actor TIM image and optionally its palette to field VRAM.
 * @param image Paletted TIM file.
 * @param image_slot Destination VRAM texture slot.
 * @param actor_index Actor whose CLUT row receives the palette.
 * @param upload_palette Nonzero to upload the TIM palette before the image.
 */
void field_upload_actor_image(FieldTimData* image, s32 image_slot, s32 actor_index, s32 upload_palette)
{
    RECT rect;
    u16 width;
    u16 height;
    s32 size;

    width = image->file.palette.rect.w;
    height = image->file.palette.rect.h;
    size = image->file.palette.size;

    if (upload_palette != 0)
    {
        lom_gpu_set_rect(&rect, 0, actor_index + FIELD_RESOURCE_CLUT_VRAM_Y, width * height, 1);
        lom_graphics_upload(&rect, (u_long*)image->file.palette.pixels);
    }

    image = (FieldTimData*)((u8*)image + size + TIM_HEADER_SIZE);
    width = image->block.rect.w;
    height = image->block.rect.h;

    if (image_slot >= FIELD_IMAGE_SLOT_SECOND_ROW)
    {
        lom_gpu_set_rect(&rect, FIELD_IMAGE_ROW1_X - ((image_slot - (FIELD_IMAGE_SLOT_SECOND_ROW - 1)) << 6), FIELD_IMAGE_ROW1_Y, width, height);
    }
    else
    {
        lom_gpu_set_rect(&rect, FIELD_IMAGE_ROW0_X - (image_slot << 6), 0, width, height);
    }

    lom_graphics_upload(&rect, (u_long*)image->block.pixels);
    lom_graphics_wait(0);
}

/**
 * @brief Copy a word-aligned byte range to the field resource cursor and advance it.
 * @param src Start of the source range.
 * @param end End of the source range (exclusive, rounded up to a word).
 */
void field_copy_scene_geometry(s32* src, s32* end)
{
    s32 size;
    s32 words;
    s32* dst;

    size = (u8*)end - (u8*)src + 3;
    words = size >> 2;
    dst = (s32*)g_field_resource_cursor;
    g_field_resource_cursor += size & ~3;

    while (words != 0)
    {
        *dst = *src;
        dst++;
        src++;
        words--;
    }
}

/**
 * @brief Install the scene's layout records and place its active actors.
 * @param layout Scene layout containing actor and action records.
 * @note Active layout records fill consecutive actor records from FIELD_PARTY_COUNT on.
 */
void field_load_scene_actors(FieldSceneLayout* layout)
{
    FieldActor* actor = &g_field_actors[FIELD_PARTY_COUNT];
    FieldObjectState* state = &g_field_object_states[FIELD_PARTY_COUNT];
    FieldLayoutRecord* layout_record = layout->records;
    s32 loaded_count = 0;
    s32 index = 0;
    s32 count = layout->count;
    s32 i;

    g_field_loaded_actor_count = FIELD_PARTY_COUNT;

    for (; index < count; index++, layout_record++)
    {
#if defined(LOM_NATIVE_POINTERS)
        state = FIELD_OBJECT_STATE_FOR_WRITE(loaded_count + FIELD_PARTY_COUNT);
        state->hp_display.word &= 0x7FFFFFFF;
        FIELD_OBJECT_STATE_COMMIT(loaded_count + FIELD_PARTY_COUNT, state);
#else
        state->hp_display.word &= 0x7FFFFFFF;
#endif
        field_install_actor_action(layout_record, index);

        if (layout_record->control.bits.active)
        {
            field_initialize_actor_record(loaded_count + FIELD_PARTY_COUNT, layout_record->source.actor + FIELD_PARTY_COUNT);
#if defined(LOM_NATIVE_POINTERS)
            actor = FIELD_ACTOR_FOR_WRITE(loaded_count + FIELD_PARTY_COUNT);
            state = FIELD_OBJECT_STATE_FOR_WRITE(loaded_count + FIELD_PARTY_COUNT);
#endif

            if (layout_record->control.bits.hidden)
            {
                actor->presence = FIELD_ACTOR_HIDDEN;
            }
            else
            {
                actor->presence = FIELD_ACTOR_VISIBLE;
            }

            actor->control.bits.golem_color = layout_record->control.bits.golem_color;
#if !defined(VERSION_JP)
            if ((g_field_pending_scene_id & FIELD_SCENE_ID_MASK) == FIELD_SCENE_NO_LAYOUT_GOLEM_COLORS)
            {
                actor->control.bits.golem_color = 0;
            }
#endif
            actor->control.bits.palette = layout_record->control.bits.palette;
            actor->x = layout_record->position.bits.x << FIELD_POSITION_SHIFT;
            actor->z = layout_record->position.bits.z << FIELD_POSITION_SHIFT;
            actor->y = layout_record->position.bits.y << FIELD_POSITION_SHIFT;
            state->contact.bits.action_pending = 0;
            state->contact.bits.targeted = 0;
            state->movement.word &= ~FIELD_MOVEMENT_INVULNERABLE;
            state->tint_red = FIELD_TARGET_OBJECT_PART(actor->object_index)->tint_red;
            state->tint_green = FIELD_TARGET_OBJECT_PART(actor->object_index)->tint_green;
            state->tint_blue = FIELD_TARGET_OBJECT_PART(actor->object_index)->tint_blue;
            state->tint_timer = 0;
            state->interaction_kind = 0;
            state->contact.bits.no_hit_test = 0;
            state->hp_display.bits.panel_timer = 0;
            state->hp_display.bits.value = state->maximum_hp;
            state->current_hp.word = state->maximum_hp & FIELD_HP_VALUE_MASK;
            state->key = index + FIELD_PARTY_COUNT;
            state->enabled_events = layout_record->enabled_events;
            state->group.flags = layout_record->control.flags;

            for (i = 0; i < FIELD_ACTION_SCRIPT_COUNT; i++)
            {
                state->scripts[i] = layout_record->scripts[i];
            }

            field_restart_actor_animation(actor);
#if defined(LOM_NATIVE_POINTERS)
            FIELD_ACTOR_COMMIT(loaded_count + FIELD_PARTY_COUNT, actor);
            FIELD_OBJECT_STATE_COMMIT(loaded_count + FIELD_PARTY_COUNT, state);
#endif
            state++;
            actor++;
            loaded_count++;
            g_field_loaded_actor_count++;
        }
    }
}

/**
 * @brief Settle every present field actor on the map collision.
 * @note Actors outside the map bounds get no contact instead.
 */
static void field_refresh_actor_collisions(void)
{
    FieldActor* actor;
    FieldObjectState* state;
    FieldMapBounds* bounds = FIELD_MAP_BOUNDS;
    FieldCollisionMover* mover = FIELD_COLLISION_MOVER;
    s32 i;

    actor = g_field_actors;
    state = g_field_object_states;

    for (i = 0; i < FIELD_ACTOR_COUNT; i++, state++, actor++)
    {
        if (actor->presence != FIELD_ACTOR_UNUSED)
        {
            if (actor->x >= 0 && actor->x < FIELD_MAP_X_LIMIT(bounds) && actor->z >= 0 && actor->z < FIELD_MAP_Z_LIMIT(bounds))
            {
                mover->x = actor->x;
                mover->height = actor->y;
                mover->z = actor->z;
                mover->footprint_width = FIELD_FOOTPRINT_LARGE_WIDTH;
                mover->mode.bits.footprint_depth = FIELD_FOOTPRINT_LARGE_DEPTH;
                mover->move_x = 0;
                mover->move_height = 0;
                mover->move_z = 0;
                mover->height_bias = FIELD_FOOTPRINT_STEP;
                mover->collision_node = FIELD_COLLISION_NODE_SEARCH;
                mover->flags = 0;
                mover->mode.bits.airborne_high = 0;
                mover->mode.bits.airborne_low = 0;
                field_collision_resolve_move(mover);
                state->collision_node = mover->collision_node;
                state->collision_flags = mover->flags;
                state->movement.half.height = mover->resolved_height >> 8;
                actor->x = mover->x;
                actor->z = mover->z;
                actor->y = mover->height;
            }
            else
            {
                state->movement.half.height = 0;
                state->collision_node = FIELD_COLLISION_NODE_SEARCH;
                state->collision_flags = 0;
            }
        }
        else
        {
            state->collision_node = FIELD_COLLISION_NODE_SEARCH;
            state->collision_flags = 0;
        }
    }
}

/**
 * @brief Apply collision-constrained motion to an actor position.
 * @param actor Actor to move; outside the map bounds it stays where it is.
 * @param motion X, Y and Z motion in fixed-point units.
 */
void field_move_actor_position(FieldActor* actor, s32* motion)
{
    FieldMapBounds* bounds = FIELD_MAP_BOUNDS;
    FieldCollisionMover* mover = FIELD_COLLISION_MOVER;
    s32 position_z;
    s32 position_x;

    position_x = actor->x;

    if (position_x < 0)
    {
        return;
    }

    if (position_x >= FIELD_MAP_X_LIMIT(bounds))
    {
        return;
    }

    position_z = actor->z;

    if (position_z < 0)
    {
        return;
    }

    if (position_z >= FIELD_MAP_Z_LIMIT(bounds))
    {
        return;
    }

    mover->x = position_x;
    mover->height = actor->y;
    mover->z = actor->z;
    mover->move_x = motion[0];
    mover->move_height = motion[1];
    mover->move_z = motion[2];

    if (g_field_object_parts[0].appearance.fields.scale_xz >= FIELD_PART_FULL_SCALE)
    {
        mover->footprint_width = FIELD_FOOTPRINT_LARGE_WIDTH;
        mover->mode.bits.footprint_depth = FIELD_FOOTPRINT_LARGE_DEPTH;
    }
    else
    {
        mover->footprint_width = FIELD_FOOTPRINT_WIDTH;
        mover->mode.bits.footprint_depth = FIELD_FOOTPRINT_DEPTH;
    }

    mover->height_bias = FIELD_FOOTPRINT_STEP;
    mover->collision_node = FIELD_COLLISION_NODE_SEARCH;
    mover->flags = 0;
    mover->mode.bits.airborne_high = 0;
    mover->mode.bits.airborne_low = 0;

    field_collision_resolve_move(mover);

    actor->x = mover->x;
    actor->z = mover->z;
    actor->y = mover->height;
}

/**
 * @brief Set the CLUT rows of the two player resources from the scene's party palette.
 */
void field_set_party_palettes(void)
{
    s32 i;

    if (g_field_party_palette_index >= FIELD_PARTY_PALETTE_COUNT)
    {
        g_field_party_palette_index = FIELD_PARTY_PALETTE_COUNT - 1;
    }

    for (i = 0; i < FIELD_PLAYER_COUNT; i++)
    {
        if (g_field_player_records[i].character_kind == FIELD_PLAYER_KIND_HERO)
        {
            g_field_resource_entries[i].attribute.palette = g_field_party_palettes[g_field_party_palette_index];
        }
        else
        {
            g_field_resource_entries[i].attribute.palette = 0;
        }
    }
}

/**
 * @brief Upload the two field transition tiles to VRAM and flip the source bank.
 */
static void field_upload_transition_tiles(void)
{
    RECT rect;
    u16* src;

    lom_gpu_set_rect(&rect, FIELD_TRANSITION_TILE_X, FIELD_TRANSITION_TILE_Y0, FIELD_TRANSITION_TILE_SIZE, FIELD_TRANSITION_TILE_SIZE);

    if (g_field_transition_tile_bank != 0)
    {
        src = g_field_transition_tiles_alt;
    }
    else
    {
        src = g_field_transition_tiles;
    }

    lom_graphics_upload(&rect, (u_long*)src);

    lom_gpu_set_rect(&rect, FIELD_TRANSITION_TILE_X, FIELD_TRANSITION_TILE_Y1, FIELD_TRANSITION_TILE_SIZE, FIELD_TRANSITION_TILE_SIZE);

    if (g_field_transition_tile_bank != 0)
    {
        src = g_field_transition_tiles_alt;
    }
    else
    {
        src = g_field_transition_tiles;
    }

    lom_graphics_upload(&rect, (u_long*)src);

    lom_graphics_wait(0);
    g_field_transition_tile_bank = g_field_transition_tile_bank == 0;
}

/**
 * @brief Recolor the transition tile pixels for a white or a black fade target.
 */
static void field_prepare_transition_tiles(void)
{
    s32 i;
    u16* p;

    g_field_transition_tile_bank = 0;

    if (g_field_fade_target.red == FIELD_FADE_WHITE && g_field_fade_target.green == g_field_fade_target.red &&
        g_field_fade_target.blue == g_field_fade_target.green)
    {
        p = g_field_transition_tiles;

        for (i = 0; i < FIELD_TRANSITION_TILE_PIXELS; i++, p++)
        {
            if ((*p & FIELD_PIXEL_COLOR_MASK) == FIELD_PIXEL_TRANSPARENT)
            {
                *p = FIELD_PIXEL_WHITE;
            }
        }
    }
    else
    {
        p = g_field_transition_tiles;

        for (i = 0; i < FIELD_TRANSITION_TILE_PIXELS; i++, p++)
        {
            if (*p == FIELD_PIXEL_WHITE)
            {
                *p = FIELD_PIXEL_TRANSPARENT;
            }
        }
    }
}
