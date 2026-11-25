#include "common.h"
#include <libgpu.h>
#include <memory.h>
#include "lom_native_graphics.h"
#include "lom_native_gpu_packets.h"

#include "../internal/field_actor.h"
#include "../internal/field_actor_palette.h"
#include "../internal/field_calls.h"
#include "../internal/field_effect_types.h"
#include "../internal/field_mesh.h"
#include "common/tim.h"
#include "main/cdrom.h"
#include "main/display.h"
#include "main/field_entry.h"

#define FIELD_RESOURCE_QUEUE_CAPACITY 16
#define FIELD_NO_RESOURCE -1

#define FIELD_SHARED_OWNER 2
#define FIELD_OWNER_COUNT 3
#define FIELD_OWNER_TEXTURE_PARTS 9
#define FIELD_OWNER_TAG(owner) ((owner) < FIELD_OWNER_COUNT ? (owner) : FIELD_SHARED_OWNER)
#define FIELD_ACTOR_MAX_MESHES 3

#define FIELD_MESH_VECTORS_PER_FACE 5
#define FIELD_MESH_FACE_SIZE sizeof(FieldMeshFace)

#define FIELD_COPY_PACKED_VECTOR(output, input) (*(output)++ = *(input)++, *(output)++ = *(input)++, *(output) = *(input)++, (output) += 2)

#define FIELD_CURVE_FORMAT_CURRENT 2U

#define FIELD_ACTOR_TEXTURE_ROW_PIXELS 64
#define FIELD_ACTOR_TIM_PIXELS (sizeof(Tim) + sizeof(TimBlock))
#define FIELD_TIM_BLOCK_HEADER ((s32)sizeof(TimBlock))

#define FIELD_SOUND_SHARED_SIZE 0x1000
#define FIELD_SOUND_COUNT_SHIFT 6
#define FIELD_PART_COUNT_MASK 0x3F

#define FIELD_CONTACT_PROBE_WIDTH 8
#define FIELD_CONTACT_PROBE_HEIGHT 16
#define FIELD_CONTACT_PROBE_DEPTH 5

typedef struct
{
    s8 bytes[18];
} FieldAnimationBaseBytes;

typedef struct
{
    s8 bytes[10];
} FieldAnimationExtensionBytes;

typedef struct
{
    s8 bytes[sizeof(FieldAnimationDef)];
} FieldAnimationDefBytes;

typedef struct
{
    s8 bytes[6];
} FieldAnimationTailBytes;

typedef struct
{
    u16 texture_offset;
    u16 has_tracks;
} FieldActorResourceHeader;

typedef union
{
    u8* bytes;
    FieldActorResourceHeader* resource_header;
    u16* halves;
    s32* words;
    uintptr_t address;
    SVECTOR* vectors;
    FieldMeshFace* faces;
    FieldAnimationDef* animation;
    FieldAnimationBaseBytes* base;
    FieldAnimationExtensionBytes* extension;
    FieldAnimationDefBytes* alternate;
    FieldAnimationTailBytes* tail;
    TimBlock* tim_block;
    TimDimensions* dimensions;
    u_long* image;
} FieldStreamPointer;

#define FIELD_ANIMATION_BLOCK_SIZE                                                                                                                             \
    (sizeof(FieldAnimationBaseBytes) + sizeof(FieldAnimationExtensionBytes) + 2 * sizeof(FieldAnimationDefBytes) + sizeof(FieldAnimationTailBytes))

typedef union
{
    struct
    {
        u8 segment_count;
        u8 segment_offset;
        u16 end_value;
        u16 start_value;
    } fields;

    u16 head;
} FieldLegacyCurve;

#define FIELD_LEGACY_CURVE(actor, index) (&((FieldLegacyCurve*)(actor)->curves)[index])

#if defined(LOM_NATIVE_POINTERS)
#include <string.h>
#else
void* memcpy(void* dst, const void* src, s32 size);
#endif

extern s32 g_field_resource_queue_count;
extern s32 g_field_resource_queue[FIELD_RESOURCE_QUEUE_CAPACITY];
extern s32 g_field_loading_resource;
extern FieldMeshTexturePart g_field_mesh_texture_parts[FIELD_OWNER_COUNT][FIELD_OWNER_TEXTURE_PARTS];
static s32 field_enqueue_resource_read(s32 resource_id);
static void field_upload_actor_texture(u8* tim, s32 owner);

/**
 * @brief Start the interaction of a marker the player walks into.
 * @param player Leading player, whose position is probed.
 * @note The interaction starts once per contact; the latch clears when the
 *       player leaves the marker or a scene request is pending.
 */
void field_check_marker_contact(FieldActor* player)
{
    struct FieldCollisionQuery probe;
    s32 label;

    if (g_field_actors[0].command == 0)
    {
        probe.x = player->x;
        probe.y = player->y;
        probe.z = player->z;
        probe.width = FIELD_CONTACT_PROBE_WIDTH;
        probe.height_tolerance = FIELD_CONTACT_PROBE_HEIGHT;
        probe.depth = FIELD_CONTACT_PROBE_DEPTH;
        label = field_collision_hit_markers(&probe);

        if (label != FIELD_COLLISION_MARKER_NONE)
        {
            if (g_field_scene_request_pending == 0)
            {
                if (g_field_scene_contact_latched == 0)
                {
                    field_start_interaction(0, label | FIELD_INTERACTION_SCRIPT);
                }

                g_field_scene_contact_latched = 1;
                return;
            }

            g_field_scene_contact_latched = 0;
            return;
        }

        g_field_scene_contact_latched = 0;
    }
}

/**
 * @brief Queue the read of an actor resource.
 * @param resource_id CD resource index to read.
 * @return 0 when queued, 1 when the queue is full.
 */
s32 field_request_resource_read(s32 resource_id)
{
    return field_enqueue_resource_read(resource_id);
}

/**
 * @brief Drop every pending resource read.
 */
void field_clear_resource_queue(void)
{
    g_field_resource_queue_count = 0;
}

/**
 * @brief Get the resource whose read was issued last.
 * @return Resource id, or FIELD_NO_RESOURCE when the queue ran empty.
 */
s32 field_get_loading_resource(void)
{
    return g_field_loading_resource;
}

/**
 * @brief Append a resource read to the pending queue.
 * @param resource_id CD resource index to read.
 * @return 0 when queued, 1 when the queue is full.
 */
static s32 field_enqueue_resource_read(s32 resource_id)
{
    s32 count;

    count = g_field_resource_queue_count;

    if (count < FIELD_RESOURCE_QUEUE_CAPACITY)
    {
        g_field_resource_queue[count] = resource_id;
        g_field_resource_queue_count = count + 1;
        return 0;
    }

    return 1;
}

/**
 * @brief Issue the read at the head of the queue into g_field_cd_buffer.
 * @note The head is removed only when the CD queue accepted the read.
 */
void field_issue_next_resource_read(void)
{
    s32 count;
    s32 i;

    if (g_field_resource_queue_count == 0)
    {
        g_field_loading_resource = FIELD_NO_RESOURCE;
        return;
    }

    if (cdrom_queue_read((u16)g_field_resource_queue[0], g_field_cd_buffer) >= 0)
    {
        g_field_loading_resource = g_field_resource_queue[0];
        count = g_field_resource_queue_count - 1;

        for (i = 0; i < count; i++)
        {
            g_field_resource_queue[i] = g_field_resource_queue[i + 1];
        }

        g_field_resource_queue_count--;
    }
}

/**
 * @brief Free every actor heap block of an owner.
 * @param tag Owner tag of the blocks to free.
 */
void field_free_owner_resources(s32 tag)
{
    field_block_free_tag(g_field_actor_heap, tag);
}

/**
 * @brief Unpack the actor resource in g_field_cd_buffer into heap blocks of an actor.
 * @param owner Owner object index; selects the heap tag, texture slot and texture part table.
 * @param actor Actor receiving the unpacked tracks, meshes, curves, parts and animations.
 */
void field_unpack_actor_resource(s32 owner, FieldActorSlot* actor)
{
    s32 mesh_count;
    FieldStreamPointer resource;
    s32 texture_total;
    FieldParameterCurve converted;
    s32 allocation_size;
    s32 has_tracks;
    s32 track_size;
    s32 curve_bytes;
    s32 segment_bytes;
    s32 part_bytes;
    s32 sound_bytes;
    s32 tag;
    s32 row;
    s32 texture_height;
    s32 item;
    s32 sound_count;
    FieldStreamPointer cursor;
    s32 i;
    s32 column;
    u16* segments;
    FieldObjectPart* parts;
    FieldAnimationDef* animation;
    FieldParameterCurve* curves;
    FieldStreamPointer output;
    u16* pixel_input;
    FieldStreamPointer row_input;
    u16* pixel_output;
    FieldStreamPointer block;
    FieldMeshResource* mesh;
    FieldMeshResource* mesh_fields;
    s32 track_offset;
    s32 j;
    FieldMeshTexturePart* texture;
    s32 curve_count;
    s32 duration;
    u8* track_source;
    FieldStreamPointer sound_table;

    block.bytes = g_field_cd_buffer;
    cursor = block;
    resource = cursor;

    if (cursor.resource_header->texture_offset != 0)
    {
        field_upload_actor_texture(cursor.bytes + cursor.resource_header->texture_offset, owner);
    }

    cursor.halves = &block.resource_header->has_tracks;
    has_tracks = *cursor.halves;
    cursor.address = ALIGN4(cursor.address + sizeof(u16));

    if (has_tracks != 0)
    {
        do
        {
            track_offset = *cursor.halves;
        } while (0);
        tag = owner;
        track_source = block.bytes + track_offset;
        track_size = cursor.halves[1] - track_offset;

        if (tag > FIELD_SHARED_OWNER)
        {
            tag = FIELD_SHARED_OWNER;
        }

        block.bytes = field_block_alloc(g_field_actor_heap, track_size, tag);
        cursor.halves++;
        memcpy(block.bytes, track_source, track_size);
        actor->track_data = block.bytes;
    }

    cursor.halves++;
    mesh_count = *cursor.halves++;

    if (mesh_count != 0)
    {
        mesh = field_block_alloc(g_field_actor_heap, FIELD_ACTOR_MAX_MESHES * sizeof(FieldMeshResource), FIELD_OWNER_TAG(owner));
        actor->meshes = mesh;
    }

    i = 0;
    texture_total = 0;

    if (mesh_count != 0)
    {
        mesh_fields = mesh;

        do
        {
            block.bytes = resource.bytes + *cursor.halves;
            mesh->face_count = *block.halves;
            block.bytes += 4;
            mesh_fields->texture_part_count = *block.bytes;
            block.bytes += 4;
            allocation_size = mesh->face_count * (FIELD_MESH_VECTORS_PER_FACE * sizeof(SVECTOR));
            output.vectors = field_block_alloc(g_field_actor_heap, allocation_size, FIELD_OWNER_TAG(owner));
            mesh_fields->vertices = output.vectors;

            for (item = 0; item < mesh->face_count * 3; item++)
            {
                FIELD_COPY_PACKED_VECTOR(output.halves, block.halves);
            }

            mesh_fields->normals = output.vectors;

            for (item = 0; item < mesh->face_count; item++)
            {
                FIELD_COPY_PACKED_VECTOR(output.halves, block.halves);
            }

            mesh_fields->offsets = output.vectors;

            for (item = 0; item < mesh->face_count; item++)
            {
                FIELD_COPY_PACKED_VECTOR(output.halves, block.halves);
            }

            allocation_size = mesh->face_count * FIELD_MESH_FACE_SIZE;
            tag = FIELD_OWNER_TAG(owner);
            item = 0;
            output.faces = field_block_alloc(g_field_actor_heap, allocation_size, tag);
            mesh_fields->faces = output.faces;

            for (; item < mesh->face_count; item++)
            {
                for (j = 0; j < FIELD_MESH_FACE_SIZE; j++, output.bytes++, block.bytes++)
                {
                    *output.bytes = *block.bytes;
                }
            }

            texture = mesh_fields->texture_parts = &g_field_mesh_texture_parts[FIELD_OWNER_TAG(owner)][texture_total];
            j = 0;

            if (mesh_fields->texture_part_count != 0)
            {
                do
                {
                    texture->x = *block.bytes++;
                    texture->y = *block.bytes++;
                    texture->width = *block.bytes++;
                    texture->height = *block.bytes++;
                    allocation_size = texture->width * texture->height * 2;
                    pixel_output = field_block_alloc(g_field_actor_heap, allocation_size, FIELD_OWNER_TAG(owner));
                    row = 0;
                    texture->pixels = pixel_output;
                    texture_height = texture->height;
                    row_input.bytes = resource.bytes + *resource.halves + FIELD_ACTOR_TIM_PIXELS;
                    row_input.halves = row_input.halves + texture->x + texture->y * FIELD_ACTOR_TEXTURE_ROW_PIXELS;

                    if (texture_height == 0)
                    {
                        j++;
                        texture++;
                        continue;
                    }

                    do
                    {
                        pixel_input = row_input.halves;

                        for (column = 0; column < texture->width; column++, pixel_output++, pixel_input++)
                        {
                            *pixel_output = *pixel_input;
                        }

                        row++;
                        row_input.halves += FIELD_ACTOR_TEXTURE_ROW_PIXELS;
                    } while (row < texture->height);

                    j++;
                    texture++;
                } while (j < mesh_fields->texture_part_count);
            }

            i++;
            cursor.halves++;
            texture_total += mesh_fields->texture_part_count;
            mesh_fields++;
            mesh++;
        } while (i < mesh_count);
    }

    duration = *cursor.halves++;
    curve_count = *cursor.bytes++;
    curve_bytes = curve_count * sizeof(FieldParameterCurve);
    curves = field_block_alloc(g_field_actor_heap, curve_bytes, FIELD_OWNER_TAG(owner));
    actor->curves = curves;
    memcpy(curves, cursor.bytes, curve_bytes);
    cursor.bytes += curve_bytes;
    j = *cursor.bytes++;
    segment_bytes = j * sizeof(u16);
    segments = field_block_alloc(g_field_actor_heap, segment_bytes, FIELD_OWNER_TAG(owner));
    actor->curve_segments = segments;
    memcpy(segments, cursor.bytes, segment_bytes);
    cursor.bytes += segment_bytes;
    j = *cursor.bytes;
    sound_count = j >> FIELD_SOUND_COUNT_SHIFT;

    if (sound_count > FIELD_ACTOR_SOUND_COUNT)
    {
        sound_count = 0;
    }

    j = j & FIELD_PART_COUNT_MASK;
    cursor.bytes++;
    actor->part_count = j;
    part_bytes = j * sizeof(FieldObjectPart);
    parts = field_block_alloc(g_field_actor_heap, part_bytes, FIELD_OWNER_TAG(owner));
    actor->parts = parts;
    memcpy(parts, cursor.bytes, part_bytes);
    cursor.bytes += part_bytes;
    actor->animation_index = 0;
    animation = field_block_alloc(g_field_actor_heap, FIELD_ANIMATION_BLOCK_SIZE, FIELD_OWNER_TAG(owner));
    actor->animation = animation;
    animation->duration = duration;
    block.animation = actor->animation;
    actor->animations = actor->animation;
    *block.base = *cursor.base;
    block.base++;
    cursor.base++;

    if (actor->animation->flags & FIELD_ANIM_OWN_DURATION)
    {
        *block.extension = *cursor.extension;
        block.extension++;
        cursor.extension++;
    }

    if (actor->parts->orientation_flags.bits.mode < FIELD_CURVE_FORMAT_CURRENT)
    {
        for (i = 0; i < curve_count; i++)
        {
            converted.head.bits.segment_count = FIELD_LEGACY_CURVE(actor, i)->fields.segment_count & FIELD_CURVE_SEGMENT_COUNT_MASK;
            converted.head.bits.random = FIELD_LEGACY_CURVE(actor, i)->head >> 15;
            converted.head.bytes.segment_offset = (FIELD_LEGACY_CURVE(actor, i)->head >> 8) & FIELD_CURVE_SEGMENT_COUNT_MASK;
            converted.start_value = FIELD_LEGACY_CURVE(actor, i)->fields.start_value;
            converted.end_value = FIELD_LEGACY_CURVE(actor, i)->fields.end_value;
            bcopy((u8*)&converted, (u8*)&actor->curves[i], sizeof(converted));
        }
    }

    actor->duration = actor->animation->duration;

    if (actor->animations->flags & FIELD_ANIMATION_HAS_ALTERNATES)
    {
        *block.alternate = *cursor.alternate;
        block.alternate++;
        cursor.alternate++;
        *block.alternate = *cursor.alternate;
        cursor.alternate++;
        block.address = ALIGN2(block.address + sizeof(FieldAnimationDefBytes));
        actor->part_masks = block.halves;
        actor->animation_index = 0;
        actor->sequence_active = 0;
        *block.tail = *cursor.tail;
    }

    if (sound_count != 0)
    {
        for (i = 0; i < sound_count; i++)
        {
            cursor.address = ALIGN4(cursor.address);
            sound_bytes = *cursor.words;

            if (sound_bytes < FIELD_SOUND_SHARED_SIZE)
            {
                actor->sound_data[i] = field_block_alloc(g_field_actor_heap, sound_bytes, FIELD_OWNER_TAG(owner));
            }
            else
            {
                sound_table.bytes = g_field_sound_tables;
                actor->sound_data[i] = (struct AkaoHeader*)(sound_table.bytes + sound_table.words[1]);
            }

            cursor.words++;

            if (sound_bytes < FIELD_SOUND_SHARED_SIZE)
            {
                memcpy(actor->sound_data[i], cursor.bytes, sound_bytes);
            }

            cursor.bytes += sound_bytes;
        }
    }
}

/**
 * @brief Upload an owner's actor TIM (and its CLUT) to the owner's VRAM slot.
 * @param tim Actor TIM image: 8bpp, optionally with a 256-entry CLUT.
 * @param owner Owner object index; owners from FIELD_SHARED_OWNER on share one slot.
 * @note The CLUT is also kept in the owner's palette buffer for palette animation.
 */
static void field_upload_actor_texture(u8* tim, s32 owner)
{
    FieldStreamPointer cursor;
    s32 clut_block_size;
    u8* clut_buffer;
    RECT rect;
    s32 width;
    s32 height;

    cursor.bytes = tim;
    cursor.words++;
    g_field_texture_slot_flags[FIELD_OWNER_TAG(owner)] = *cursor.bytes;
    cursor.words++;

    if (g_field_texture_slot_flags[FIELD_OWNER_TAG(owner)] & TIM_FLAG_HAS_CLUT)
    {
        clut_block_size = cursor.tim_block->bnum;
        cursor.tim_block++;

        if (owner < FIELD_SHARED_OWNER)
        {
            lom_gpu_set_rect(&rect, 0, FIELD_ACTOR_CLUT_VRAM_Y + owner * 2, CLUT_ENTRY_COUNT, 1);
            clut_buffer = g_field_actor_clut_buffers;
            memcpy(clut_buffer + owner * FIELD_ACTOR_CLUT_BUFFER_SIZE, cursor.bytes, MIN(clut_block_size - FIELD_TIM_BLOCK_HEADER, FIELD_CLUT_BYTES));
        }
        else
        {
            lom_gpu_set_rect(&rect, 0, VRAM_CLUT_Y, CLUT_ENTRY_COUNT, 1);
            memcpy(g_field_shared_clut_buffer, cursor.bytes, MIN(clut_block_size - FIELD_TIM_BLOCK_HEADER, FIELD_CLUT_BYTES));
        }

        lom_graphics_upload(&rect, cursor.image);
        cursor.bytes = cursor.bytes + clut_block_size - FIELD_TIM_BLOCK_HEADER;
    }

    cursor.dimensions = &cursor.tim_block->dimensions;
    width = *cursor.halves++;
    height = *cursor.halves++;

    if (owner < FIELD_SHARED_OWNER)
    {
        lom_gpu_set_rect(&rect, FIELD_ACTOR_TEXTURE_VRAM_X + owner * FIELD_ACTOR_TEXTURE_VRAM_WIDTH, FIELD_ACTOR_TEXTURE_VRAM_Y, width, height);
    }
    else
    {
        lom_gpu_set_rect(&rect, FIELD_SHARED_TEXTURE_VRAM_X, FIELD_SHARED_TEXTURE_VRAM_Y, width, height);
    }

    lom_graphics_upload(&rect, cursor.image);
}
