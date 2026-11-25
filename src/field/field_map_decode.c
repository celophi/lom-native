#include "lom_native_field_map.h"

#include "common.h"
#include "overlays/field/internal/field_scene_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_PS1_ADDRESS 0x80180000u

#define PS1_HEADER_SIZE 0x24
#define PS1_MAP_OBJECT_SIZE 0x34
#define PS1_OBJ_DEF_SIZE 0x20
#define PS1_PART_DEF_SIZE 0x20
#define PS1_NODE_DEF_SIZE 0x18
#define PS1_MARKER_DEF_SIZE 0x18
#define PS1_ANIM_DEF_SIZE 0x18
#define PS1_POINTER_LIST_LIMIT 256

#define ARENA_BYTES (128 * 1024)
#define RECORD_SLOTS 4096

typedef enum
{
    RECORD_LIST,
    RECORD_MAP_OBJECT,
    RECORD_OBJ_DEF,
    RECORD_PART_DEF,
    RECORD_NODE_DEF,
    RECORD_MARKER_DEF,
    RECORD_ANIM_DEF,
    RECORD_HEADER_REC,
} RecordType;

typedef struct
{
    uint32_t address;
    RecordType type;
    void* native;
} RecordSlot;

_Alignas(16) unsigned char lom_native_field_map_header[sizeof(FieldResource) + sizeof(FieldMapObject**)];

static _Alignas(16) unsigned char s_arena[ARENA_BYTES];
static size_t s_arena_used;
static RecordSlot s_records[RECORD_SLOTS];
static unsigned s_record_count;

static const unsigned char* s_file;
static uint32_t s_records_end;

/**
 * @brief Stop because a map's data can't be decoded.
 * @param what What went wrong.
 * @param address The PS1 address it was at.
 */
static _Noreturn void fail(const char* what, uint32_t address)
{
    fprintf(stderr, "[lom-native] field map decode: %s at 0x%08X\n", what, address);
    abort();
}

/**
 * @brief Read a little-endian 32-bit word from the map file.
 * @param bytes The word's first byte.
 * @return The word.
 */
static uint32_t read_u32(uint32_t address)
{
    const unsigned char* bytes = s_file + (address - MAP_PS1_ADDRESS);
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

/**
 * @brief Find a PS1 address's bytes in the map file.
 * @param address The address, inside the file.
 * @return Its bytes.
 */
static const unsigned char* file_bytes(uint32_t address)
{
    return s_file + (address - MAP_PS1_ADDRESS);
}

/**
 * @brief Take room for a native record from the decoder's arena, 16-byte
 *        aligned. Stops FIELD when it's full.
 * @param size The record's size.
 * @return The room, zeroed.
 */
static void* arena_take(size_t size)
{
    size = (size + 15) & ~(size_t)15;
    if (s_arena_used + size > sizeof(s_arena))
    {
        fail("the map's records don't fit the arena", 0);
    }
    void* record = s_arena + s_arena_used;
    s_arena_used += size;
    memset(record, 0, size);
    return record;
}

/**
 * @brief Turn a PS1 address in a record into the host address of the map file's
 *        bytes there, for data that keeps its PS1 layout. Stops FIELD for one
 *        outside the file.
 * @param address The address; 0 gives NULL.
 * @param what What it points at, for the error.
 * @return The host address.
 */
static void* data_pointer(uint32_t address, const char* what)
{
    if (address == 0)
    {
        return NULL;
    }
    if (address < MAP_PS1_ADDRESS || address >= MAP_PS1_ADDRESS + 0x80000u)
    {
        fail(what, address);
    }
    return (void*)(s_file + (address - MAP_PS1_ADDRESS));
}

/**
 * @brief Find the native record already decoded from a PS1 address, so records
 *        the map shares are decoded once. Stops FIELD when the address was
 *        decoded as another type.
 * @param address The record's PS1 address.
 * @param type What it should be.
 * @return The native record, or NULL when it hasn't been decoded.
 */
static void* find_record(uint32_t address, RecordType type)
{
    for (unsigned i = 0; i < s_record_count; i++)
    {
        if (s_records[i].address == address)
        {
            if (s_records[i].type != type)
            {
                fail("an address reached as two kinds of record", address);
            }
            return s_records[i].native;
        }
    }
    return NULL;
}

/**
 * @brief Remember a record that's been decoded, so a second pointer to it finds
 *        the same one.
 * @param address Its PS1 address.
 * @param type What kind of record it is.
 * @param native Where the decoded record is.
 */
static void remember(uint32_t address, RecordType type, void* native)
{
    if (s_record_count == RECORD_SLOTS)
    {
        fail("more records than the decoder keeps", address);
    }
    s_records[s_record_count++] = (RecordSlot){address, type, native};
}

/**
 * @brief Stop FIELD for a record that isn't wholly inside the map's records.
 * @param address The record's PS1 address.
 * @param size Its size.
 * @param what What it is, for the error.
 */
static void check_record(uint32_t address, uint32_t size, const char* what)
{
    if (address < MAP_PS1_ADDRESS + PS1_HEADER_SIZE || address + size > s_records_end)
    {
        fail(what, address);
    }
}

typedef void* (*Decoder)(uint32_t address);

static void* decode_part_def(uint32_t address);
static void* decode_obj_def(uint32_t address);
static void* decode_node_def(uint32_t address);
static void* decode_marker_def(uint32_t address);

/**
 * @brief Decode a NULL-ended list of PS1 pointers, decoding each record it
 *        points at.
 * @param address The list's PS1 address; 0 gives NULL.
 * @param decode Decodes one record.
 * @param what What the list holds, for the errors.
 * @return The native list, NULL-ended.
 */
static void** decode_list(uint32_t address, Decoder decode, const char* what)
{
    if (address == 0)
    {
        return NULL;
    }
    void** native = find_record(address, RECORD_LIST);
    if (native != NULL)
    {
        return native;
    }
    unsigned count = 0;
    while (count < PS1_POINTER_LIST_LIMIT)
    {
        check_record(address + count * 4, 4, what);
        if (read_u32(address + count * 4) == 0)
        {
            break;
        }
        count++;
    }
    native = arena_take((count + 1) * sizeof(void*));
    remember(address, RECORD_LIST, native);
    for (unsigned i = 0; i < count; i++)
    {
        native[i] = decode(read_u32(address + i * 4));
    }
    return native;
}

/**
 * @brief Decode an object part's definition, and everything it points at.
 * @param address Its PS1 address; 0 gives NULL.
 * @return The native FieldPartDef.
 */
static void* decode_part_def(uint32_t address)
{
    FieldPartDef* native = find_record(address, RECORD_PART_DEF);
    if (native != NULL || address == 0)
    {
        return native;
    }
    check_record(address, PS1_PART_DEF_SIZE, "a part definition outside the records");
    native = arena_take(sizeof(*native));
    remember(address, RECORD_PART_DEF, native);
    native->tiles = data_pointer(read_u32(address), "part definition tiles");
    _Static_assert(offsetof(FieldPartDef, clut_tr) + 2 - offsetof(FieldPartDef, _pad0) == PS1_PART_DEF_SIZE - 4, "FieldPartDef's plain fields");
    memcpy(native->_pad0, file_bytes(address + 4), PS1_PART_DEF_SIZE - 4);
    return native;
}

/**
 * @brief Decode an object's definition, with its parts and nodes.
 * @param address Its PS1 address; 0 gives NULL.
 * @return The native FieldObjDef.
 */
static void* decode_obj_def(uint32_t address)
{
    FieldObjDef* native = find_record(address, RECORD_OBJ_DEF);
    if (native != NULL || address == 0)
    {
        return native;
    }
    check_record(address, PS1_OBJ_DEF_SIZE, "an object definition outside the records");
    native = arena_take(sizeof(*native));
    remember(address, RECORD_OBJ_DEF, native);
    native->part_defs = (FieldPartDef**)decode_list(read_u32(address), decode_part_def, "a part definition list");
    native->shared_source = data_pointer(read_u32(address + 4), "object definition palette");
    _Static_assert(sizeof(FieldObjDef) - offsetof(FieldObjDef, _pad1) >= PS1_OBJ_DEF_SIZE - 8, "FieldObjDef's plain fields");
    memcpy(native->_pad1, file_bytes(address + 8), PS1_OBJ_DEF_SIZE - 8);
    return native;
}

/**
 * @brief Decode an attached node's definition.
 * @param address Its PS1 address; 0 gives NULL.
 * @return The native FieldNodeDef.
 */
static void* decode_node_def(uint32_t address)
{
    FieldNodeDef* native = find_record(address, RECORD_NODE_DEF);
    if (native != NULL || address == 0)
    {
        return native;
    }
    check_record(address, PS1_NODE_DEF_SIZE + 4, "a node definition outside the records");
    unsigned runs = 0;
    do
    {
        check_record(address + PS1_NODE_DEF_SIZE + runs * 4, 4, "a node run outside the records");
    } while (read_u32(address + PS1_NODE_DEF_SIZE + runs++ * 4) != 0);
    native = arena_take(offsetof(FieldNodeDef, runs) + runs * sizeof(FieldNodeRun));
    remember(address, RECORD_NODE_DEF, native);
    _Static_assert(offsetof(FieldNodeDef, runs) - offsetof(FieldNodeDef, flags) == PS1_NODE_DEF_SIZE - 4, "FieldNodeDef's plain fields");
    memcpy(&native->flags, file_bytes(address + 4), PS1_NODE_DEF_SIZE - 4 + runs * sizeof(FieldNodeRun));
    native->next = decode_node_def(read_u32(address));
    return native;
}

/**
 * @brief Decode a marker's definition.
 * @param address Its PS1 address; 0 gives NULL.
 * @return The native FieldMarkerDef.
 */
static void* decode_marker_def(uint32_t address)
{
    FieldMarkerDef* native = find_record(address, RECORD_MARKER_DEF);
    if (native != NULL || address == 0)
    {
        return native;
    }
    check_record(address, PS1_MARKER_DEF_SIZE, "a marker definition outside the records");
    native = arena_take(sizeof(*native));
    remember(address, RECORD_MARKER_DEF, native);
    _Static_assert(sizeof(FieldMarkerDef) - offsetof(FieldMarkerDef, x0) >= PS1_MARKER_DEF_SIZE - 4, "FieldMarkerDef's plain fields");
    memcpy(&native->x0, file_bytes(address + 4), PS1_MARKER_DEF_SIZE - 4);
    native->next = decode_marker_def(read_u32(address));
    return native;
}

/**
 * @brief Measure the keyframe spans that follow an animation's definition, up
 *        to the spans that cover all its keyframes.
 * @param address The animation's PS1 address.
 * @param keyframes How many keyframes it has.
 * @return The spans' size in bytes.
 */
static uint32_t span_bytes(uint32_t address, uint32_t keyframes)
{
    const unsigned char* head = file_bytes(address);
    uint32_t covered = head[0] & FIELD_SPAN_COUNT_MASK;
    uint32_t bytes = 0;
    while (covered < keyframes)
    {
        uint32_t span = address + PS1_ANIM_DEF_SIZE + bytes;
        if (span + 4 > s_records_end)
        {
            fail("an animation's spans run past the records", address);
        }
        covered += file_bytes(span)[0] & FIELD_SPAN_COUNT_MASK;
        bytes += 4;
        if (bytes > 0x400)
        {
            fail("an animation with more spans than any map has", address);
        }
    }
    return bytes;
}

/**
 * @brief Decode a scene header record.
 * @param address Its PS1 address; 0 gives NULL.
 * @return The native FieldHeaderRec.
 */
static void* decode_header_record(uint32_t address)
{
    FieldHeaderRec* native = find_record(address, RECORD_HEADER_REC);
    if (native != NULL || address == 0)
    {
        return native;
    }
    check_record(address, 4 + sizeof(native->body), "a header record outside the records");
    native = arena_take(sizeof(*native));
    remember(address, RECORD_HEADER_REC, native);
    memcpy(&native->body, file_bytes(address + 4), sizeof(native->body));
    native->next = decode_header_record(read_u32(address));
    return native;
}

/**
 * @brief Decode an animation's definition, with its keyframe spans.
 * @param address Its PS1 address; 0 gives NULL.
 * @param list Which of the part's animation lists it's in.
 * @return The native FieldAnimDef.
 */
static void* decode_anim(uint32_t address, unsigned list)
{
    FieldAnimDef* native = find_record(address, RECORD_ANIM_DEF);
    if (native != NULL || address == 0)
    {
        return native;
    }
    check_record(address, PS1_ANIM_DEF_SIZE, "an animation definition outside the records");

    const unsigned char* bytes = file_bytes(address);
    uint32_t flags = read_u32(address + 4);
    unsigned kind = flags & FIELD_ANIM_KIND_MASK;
    uint32_t keyframes = ((flags >> 8) & 0xFF) + 1;
    if (list == FIELD_ANIM_GROUP_TILE && kind == FIELD_TILE_ANIM_MOVIE && bytes[1] + 1u > keyframes)
    {
        keyframes = bytes[1] + 1u;
    }
    uint32_t spans = span_bytes(address, keyframes);

    native = arena_take(sizeof(*native) + spans);
    remember(address, RECORD_ANIM_DEF, native);
    memcpy(&native->head, bytes, 4);
    memcpy(&native->flags, bytes + 4, 4);
    memcpy(native + 1, bytes + PS1_ANIM_DEF_SIZE, spans);

    uint32_t grid = read_u32(address + 0x10);
    uint32_t data = read_u32(address + 0x14);
    switch (list)
    {
    case FIELD_ANIM_GROUP_PALETTE:
        memcpy(&native->u.clut, bytes + 0xC, 8);
        if (kind == FIELD_PALETTE_ANIM_CEL_CLUT)
        {
            native->data = decode_part_def(data);
        }
        else if (kind == FIELD_PALETTE_ANIM_CEL_LIST_CLUT)
        {
            native->data = decode_obj_def(data);
        }
        else
        {
            native->data = data_pointer(data, "palette animation data");
        }
        break;
    case FIELD_ANIM_GROUP_TINT:
        memcpy(&native->u.tint, bytes + 0xC, 4);
        native->u.tint.grid = kind == FIELD_TINT_ANIM_CEL_LIST ? decode_obj_def(grid) : decode_part_def(grid);
        native->data = data_pointer(data, "tint animation data");
        break;
    default:
        memcpy(&native->u.tile, bytes + 0xC, 4);
        if (list == FIELD_ANIM_GROUP_TILE && kind == FIELD_TILE_ANIM_TWEEN_OBJECT)
        {
            native->u.tile.grid = decode_obj_def(grid);
        }
        else
        {
            native->u.tile.grid = decode_part_def(grid);
        }
        native->data = data_pointer(data, "tile animation data");
        break;
    }

    native->next = decode_anim(read_u32(address + 8), list);
    return native;
}

/**
 * @brief Decode one of the map's objects, with its definition.
 * @param address Its PS1 address; 0 gives NULL.
 * @return The native FieldMapObject.
 */
static void* decode_map_object(uint32_t address)
{
    FieldMapObject* native = find_record(address, RECORD_MAP_OBJECT);
    if (native != NULL || address == 0)
    {
        return native;
    }
    check_record(address, PS1_MAP_OBJECT_SIZE, "a map object outside the records");
    native = arena_take(sizeof(*native));
    remember(address, RECORD_MAP_OBJECT, native);
    native->object_defs = (FieldObjDef**)decode_list(read_u32(address), decode_obj_def, "an object definition list");
    native->image = data_pointer(read_u32(address + 4), "map object image");
    native->node_defs = decode_node_def(read_u32(address + 8));
    native->edge_defs = decode_marker_def(read_u32(address + 0xC));
    native->records = decode_header_record(read_u32(address + 0x10));
    for (unsigned list = 0; list < 4; list++)
    {
        native->anim_defs[list] = decode_anim(read_u32(address + 0x14 + list * 4), list);
    }
    _Static_assert(offsetof(FieldMapObject, depth) + 2 - offsetof(FieldMapObject, _pad1) == PS1_MAP_OBJECT_SIZE - 0x24, "FieldMapObject's plain fields");
    memcpy(native->_pad1, file_bytes(address + 0x24), PS1_MAP_OBJECT_SIZE - 0x24);
    return native;
}

void lom_native_field_decode_map(const void* file)
{
    s_file = file;
    s_arena_used = 0;
    s_record_count = 0;

    FieldResource* header = (FieldResource*)lom_native_field_map_header;
    memset(lom_native_field_map_header, 0, sizeof(lom_native_field_map_header));
    memcpy(header->_pad0, file_bytes(MAP_PS1_ADDRESS), 0x14);

    uint32_t texture = read_u32(MAP_PS1_ADDRESS + 0x14);
    s_records_end = texture;
    if (texture < MAP_PS1_ADDRESS + PS1_HEADER_SIZE || texture >= MAP_PS1_ADDRESS + 0x80000u)
    {
        fail("a map whose texture isn't in the load buffer", texture);
    }
    header->scene = data_pointer(texture, "map texture");
    header->seq_defs = data_pointer(read_u32(MAP_PS1_ADDRESS + 0x18), "sequence definitions");
    header->points = data_pointer(read_u32(MAP_PS1_ADDRESS + 0x1C), "points");
    *(FieldMapObject***)(header + 1) = (FieldMapObject**)decode_list(read_u32(MAP_PS1_ADDRESS + 0x20), decode_map_object, "the map object list");
}

unsigned lom_native_field_map_record_count(void)
{
    return s_record_count;
}

unsigned long lom_native_field_map_arena_bytes(void)
{
    return s_arena_used;
}
