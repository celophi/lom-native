#ifndef FIELD_SCENE_INTERNAL_H
#define FIELD_SCENE_INTERNAL_H

#include "common.h"
#include <libgpu.h>
#include <libgte.h>

#include "common/gpu_packet.h"
#include "field_animation.h"
#include "overlays/field/field_scene.h"
#include "overlays/field/field_workspace.h"
#include "overlays/movie/movie.h"

#define FIELD_COPY_TINT_COLOR(destination, source) ((destination)->rg = (source)->rg, (destination)->b = (source)->b)

#define HALF_TOWARD_ZERO(v) ((v) >= 0 ? ((v) >> 1) : (((v) + 1) >> 1))

#define SHIFT_TOWARD_ZERO(v, n) ((v) >= 0 ? ((v) >> (n)) : (((v) + ((1 << (n)) - 1)) >> (n)))

#define FIELD_TILE_PRESENT 0x80
#define FIELD_TILE_CLUT_MASK 0x1F
#define FIELD_TILE_CLUT_X_SLOT_MASK 0xF
#define FIELD_TILE_TPAGE_SLOT_MASK 0xF
#define FIELD_TILE_U_MASK 0xF
#define FIELD_TILE_V_MASK 0xF0
#define FIELD_TILE_SIZE 0x10
#define FIELD_TILE_SEMITRANS 0x40
#define FIELD_TILE_LOWER_BANK_SLOTS 0xA
#define FIELD_TILE_UPPER_BANK_VRAM_X 0x200
#define FIELD_TILE_UPPER_BANK_VRAM_X_BIAS 0x80
#define FIELD_TILE_LOWER_BANK_VRAM_X 0x140
#define FIELD_TILE_LOWER_BANK_VRAM_Y 0x100
#define FIELD_TILE_CLUT_VRAM_Y 0x1D8
#define FIELD_TILE_TPAGE_X_SHIFT 6
#define FIELD_TILE_4BIT_U_PAGE_SHIFT 4
#define FIELD_TILE_UPPER_BANK_U_CELLS_PER_SLOT 4
#define FIELD_TEXTURE_4_BIT 0
#define FIELD_TEXTURE_8_BIT 1

#define FIELD_TILE_4BIT_CLUT_X(ref) (((ref) & FIELD_TILE_CLUT_X_SLOT_MASK) << 4)
#define FIELD_TILE_4BIT_CLUT_Y(ref) (FIELD_TILE_CLUT_VRAM_Y + (((ref) & FIELD_TILE_CLUT_MASK) >> 4))
#define FIELD_TILE_8BIT_CLUT_Y(ref) (FIELD_TILE_CLUT_VRAM_Y + ((ref) & FIELD_TILE_CLUT_MASK))

#define FIELD_TILE_ABR(attrs) ((attrs) >> 4)
#define FIELD_TILE_LOWER_BANK_PAGE_X(slot) (FIELD_TILE_LOWER_BANK_VRAM_X + ((slot) << FIELD_TILE_TPAGE_X_SHIFT))
#define FIELD_TILE_UPPER_BANK_PAGE_X(slot) ((s32)(((slot) << FIELD_TILE_TPAGE_X_SHIFT) - FIELD_TILE_UPPER_BANK_VRAM_X_BIAS))
#define FIELD_TILE_TPAGE_COLUMN(slot, u_cell, depth) ((slot) + ((u_cell) >> (FIELD_TILE_4BIT_U_PAGE_SHIFT - (depth))))
#define FIELD_TILE_UPPER_BANK_U_REBASE(slot) ((FIELD_TILE_LOWER_BANK_SLOTS - (slot)) * FIELD_TILE_UPPER_BANK_U_CELLS_PER_SLOT)

#define FIELD_TILE_REC_SHARED_RGB_CODE 1
#define FIELD_TILE_REC_SHARED_TPAGE 2

#define FIELD_TILE_COLOR_WORDS ((s32*)SCRATCHPAD_ADDRESS)

typedef struct
{
    u8 clut_slot;
    u8 texture_attrs;
    u8 packed_uv;
    u8 color_index;
} FieldTileDesc;

typedef union
{
    s32 word;

    struct
    {
        u8 low_flags;
        u8 node_count;
        u8 drift_speed;
        u8 drift_angle;
    } b;
} FieldObjFlags;

typedef struct FieldPartDef FieldPartDef;
typedef struct FieldNodeDef FieldNodeDef;
typedef struct FieldMarkerDef FieldMarkerDef;
typedef struct FieldAnimDef FieldAnimDef;

typedef struct
{
    FieldPartDef** part_defs;
    u8* shared_source;
    u8 _pad1[0xC - 8];

    union
    {
        u32 word;
        u8 low;
    } flags;

    u16 scale_x;
    u16 scale_y;
    u16 scale_z;
    s16 x;
    s16 y;
    s16 z;

    union
    {
        u32 word;

        struct
        {
            u8 scroll_scale_x;
            u8 scroll_scale_y;
            u8 drift_speed;
            u8 drift_angle;
        } b;
    } motion;
} FieldObjDef;

#define FIELD_OBJ_DEF_SCREEN_FIXED 2

struct FieldPartDef
{
    FieldTileDesc* tiles;
    u8 _pad0[8 - 4];

    union
    {
        u32 word;

        struct
        {
            u8 flags;
            u8 _pad1;
            u8 cols;
            u8 rows;
        } b;
    } u;

    s16 x;
    s16 y;
    s16 z;
    u16 sweep_period;
    u8 _pad2[0x18 - 0x14];
    s16 clut_bl;
    s16 clut_tl;
    s16 clut_br;
    s16 clut_tr;
};

#define FIELD_PART_DEF_VISIBLE 0x1
#define FIELD_PART_DEF_KIND_MASK 0xE
#define FIELD_PART_DEF_DEPTH(word) (((word) >> 4) & 3)
#define FIELD_PART_DEF_DEPTH_CLUT 0x40
#define FIELD_PART_DEF_UNSHARED 0x80
#define FIELD_PART_DEF_CELLS_MASK 0xF00
#define FIELD_PART_DEF_SINGLE_CELL 0x100

#define FIELD_PART_SWEEP_MASK 0xF000
#define FIELD_PART_SWEEP_MODE(def) (((def)->u.word >> 12) & 0xF)
#define FIELD_PART_SWEEP_MODE_COUNT 5

typedef struct FieldPart FieldPart;

struct FieldPart
{
    FieldPart* next;
    FieldPartDef* def;
    FieldPart* shared;
    u32* bits;
    u8* records;
    s32 bits_size;
    s32 tpage_word;
    s32 code_word;
    u8 visible;
    u8 kind;
    u8 node_count;
    u8 _pad2[0x26 - 0x23];
    u16 instance_count;
    s32 x;
    s32 y;
    s32 z;
    s16 unused_34;
    u16 sweep_period;
    u16 sweep_phase;
    u16 row_angle;
    u16 column_angle;
    u16 rotation_angle;
    u16 scale_x;
    u16 scale_y;
    s16 clut_bl;
    s16 clut_tl;
    s16 clut_br;
    s16 clut_tr;
};

typedef struct FieldObj FieldObj;

struct FieldObj
{
    FieldObj* next;
    FieldObjDef* def;
    FieldPart* parts;
    FieldObjFlags flags;

    union
    {
        struct
        {
            u16 red;
            u16 green;
        } c;

        s32 word;
    } red_green;

    u16 blue;
    u16 red_scale;
    u16 green_scale;
    u16 blue_scale;
    s32 x;
    s32 y;
    s32 z;
    s32 drift_x;
    s32 drift_y;
};

typedef struct FieldHeaderRec FieldHeaderRec;

struct FieldHeaderRec
{
    FieldHeaderRec* next;
    s16 body[4];
};

#if defined(LOM_NATIVE_POINTERS)
typedef struct FieldSceneHeader
{
    struct FieldObjDef** unused_object_defs;
    u16* pixel_data;
    struct FieldNodeDef* unused_node_defs;
    struct FieldMarkerDef* unused_edge_defs;
    FieldHeaderRec* records;
    struct FieldAnimDef* unused_anim_defs[4];
    u8 _pad2[4];
    u16 pixel_stride;
    u8 _pad3[2];
    s32 flags;
    s16 map_width;
    s16 map_depth;
} FieldSceneHeader;
#else
typedef struct FieldSceneHeader
{
    u8 _pad0[4];
    u16* pixel_data;
    u8 _pad1[0x10 - 8];
    FieldHeaderRec* records;
    u8 _pad2[0x28 - 0x14];
    u16 pixel_stride;
    u8 _pad3[0x2C - 0x2A];
    s32 flags;
    s16 map_width;
    s16 map_depth;
} FieldSceneHeader;
#endif

#define FIELD_SCENE_HEADER_BOUNDED 0x2

struct FieldMarkerDef
{
    FieldMarkerDef* next;
    u16 x0;
    u16 y0;
    u16 x1;
    u16 y1;
    u16 offset_x;
    u16 offset_y;
    s16 depth_bias;
    s16 band_height;
    u16 label;
};

typedef struct FieldMarker FieldMarker;

struct FieldMarker
{
    FieldMarker* next;
    FieldMarkerDef* def;
    u16 x2;
    u16 y2;
    u16 x3;
    u16 y3;
    s16 x_max;
    s16 x_min;
    s16 y_max;
    s16 y_min;
    s32 side_dx;
    s32 side_dy;
    s32 edge_dx;
    s32 edge_dy;
    s32 side_lo;
    s32 side_hi;
    s32 edge_lo;
    s32 edge_hi;
};

typedef union
{
    u32 word;

    struct
    {
        u8 kind_flags;
        u8 last_frame;
        u8 frame_count;
        u8 handler_group;
    } b;
} FieldAnimDefFlags;

struct FieldAnimDef
{
    union
    {
        u32 word;

        struct
        {
            u8 span_count;
            u8 start_frame;
            u16 duration;
        } b;
    } head;

    FieldAnimDefFlags flags;
    FieldAnimDef* next;

    union
    {
        struct
        {
            u8 rect_x;
            u8 rect_y;
            u8 rect_width;
            u8 rect_height;
            FieldPartDef* grid;
        } tile;

        struct
        {
            u8 clut_mode;
            u8 reverse;
            u8 clut_slot;
            u8 clut_offset;
            u8 length;
            u8 _pad0;
            u16 pixel_offset;
        } clut;

        struct
        {
            u8 first_slot;
            u8 last_slot_offset;
            u8 _pad1[2];
            FieldPartDef* grid;
        } tint;
    } u;

    u8* data;
};

typedef struct
{
    u16 rg;
    u8 b;
    u8 code;
} FieldTintColor;

#define FIELD_TINT_COLORS ((FieldTintColor*)FIELD_TILE_COLOR_WORDS)

typedef struct
{
    u8 _pad0[4];
    u16 rg;
    u8 b;
} FieldCellTint;

typedef struct
{
    u8 _pad0[4];
    u16* data;
} FieldTintPal;

typedef struct FieldTintSrc FieldTintSrc;

struct FieldTintSrc
{
    FieldTintSrc* next;
    FieldTintPal* palette;
    FieldPart* cels;
    u8 _pad1[0x10 - 0xC];
    u16 red;
    u16 green;
    u16 blue;
    u16 red_scale;
    u16 green_scale;
    u16 blue_scale;
};

enum
{
    FIELD_TILE_ANIM_BLIT = 0,
    FIELD_TILE_ANIM_CEL_RECORDS = 1,
    FIELD_TILE_ANIM_CEL_CYCLE = 2,
    FIELD_TILE_ANIM_UPLOAD = 3,
    FIELD_TILE_ANIM_MOVIE = 4,
    FIELD_TILE_ANIM_TWEEN_PART = 5,
    FIELD_TILE_ANIM_TWEEN_OBJECT = 6,
    FIELD_TILE_ANIM_SOUND = 7
};

enum
{
    FIELD_PALETTE_ANIM_CEL_CLUT = 0,
    FIELD_PALETTE_ANIM_CEL_LIST_CLUT = 1,
    FIELD_PALETTE_ANIM_CYCLE = 2,
    FIELD_PALETTE_ANIM_CLUT_ROW = 3,
    FIELD_PALETTE_ANIM_CLUT_BLOCK = 4,
    FIELD_PALETTE_ANIM_BLEND = 5
};

enum
{
    FIELD_TINT_ANIM_CEL = 0,
    FIELD_TINT_ANIM_CEL_LIST = 1
};

#define FIELD_ANIM_KIND(def) ((def)->flags.word & FIELD_ANIM_KIND_MASK)
#define FIELD_ANIM_IS_TWEEN(def) ((FIELD_ANIM_KIND(def) >= FIELD_TILE_ANIM_TWEEN_PART) && (FIELD_ANIM_KIND(def) <= FIELD_TILE_ANIM_TWEEN_OBJECT))

#define FIELD_ANIM_GROUP_TILE 0
#define FIELD_ANIM_GROUP_PALETTE 1
#define FIELD_ANIM_GROUP_TINT 2
#define FIELD_ANIM_GROUP_EFFECT 3
#define FIELD_ANIM_GROUP_KIND_MASK 0xFF000007
#define FIELD_ANIM_GROUP_KIND(group, kind) (((group) << 24) | (kind))
#define FIELD_ANIM_PALETTE_BLEND_WORD FIELD_ANIM_GROUP_KIND(FIELD_ANIM_GROUP_PALETTE, FIELD_PALETTE_ANIM_BLEND)

typedef enum
{
    FIELD_CEL_KIND_SPRITE_16 = 0,
    FIELD_CEL_KIND_SPRITE = 1,
    FIELD_CEL_KIND_QUAD_FIRST = 2,
    FIELD_CEL_KIND_GOURAUD_QUAD = 6
} FieldCelKind;

#define FIELD_CEL_RECORD_SIZE 12
#define FIELD_CEL_SHARED_WORD_SIZE 4
#define FIELD_TILE_ANIMATED 0x80

#define FIELD_SFX_KEY_KIND_MASK 7
#define FIELD_SFX_KEY_SOUND 1
#define FIELD_SFX_CHANNEL_MASK 0x1F00
#define FIELD_SFX_FIXED_PAN 0x4000
#define FIELD_SFX_PLAY 0x8000
#define FIELD_SFX_ONE_SHOT 0x8000
#define FIELD_SFX_VOLUME(key) (((key)->sound.word >> 8) & 0x7F)

#define FIELD_AXIS_X 0
#define FIELD_AXIS_Y 1
#define FIELD_AXIS_Z 2

#define FIELD_OBJ_VISIBLE 1

#define FIELD_SEQ_PHASE_MASK 3
#define FIELD_SEQ_PHASE_RUNNING 1
#define FIELD_SEQ_PHASE_FINISHED 2
#define FIELD_SEQ_NO_LINK 0xFF
#define FIELD_ANIM_STATE_RUNNING 0
#define FIELD_ANIM_STATE_STOPPING 1
#define FIELD_ANIM_STATE_FINISHED 2
#define FIELD_ANIM_STATE_MOVIE_STARTING 3

#define FIELD_ANIM_FLAG_PING_PONG 0x01
#define FIELD_ANIM_FLAG_STOP_AT_KEYFRAME 0x02
#define FIELD_ANIM_FLAG_REVERSE 0x04
#define FIELD_ANIM_FLAG_START_PENDING 0x08
#define FIELD_ANIM_FLAG_SECOND_BUFFER 0x10
#define FIELD_ANIM_FLAG_UPLOAD_PENDING 0x20
#define FIELD_ANIM_FLAG_ACTIVE 0x40

#define FIELD_ANIM_KIND_MASK 0x07
#define FIELD_ANIM_DEF_PING_PONG 0x08
#define FIELD_ANIM_DEF_IGNORE_REPEAT_COUNT 0x10
#define FIELD_ANIM_DEF_TIMED 0x20
#define FIELD_ANIM_DEF_SPAN_INDEXED 0x40
#define FIELD_ANIM_DEF_ACTIVE 0x80

typedef union
{
    s32 word;

    struct
    {
        u8 control;
        u8 state;
        u8 keyframe;
        u8 stop_keyframe;
    } b;
} FieldAnimFlags;

typedef struct FieldAnim FieldAnim;

struct FieldAnim
{
    FieldAnim* next;
    FieldAnimDef* def;
    u8 _pad0[0xC - 8];
    FieldPart* cels;

    union
    {
        FieldTintSrc* tint_src;
        FieldObj* object;
        u8* tiles;
    } owner;

    s32 tween_x;
    s32 tween_y;
    s32 tween_z;
    u8* frame_data;
    FieldAnimFlags flags;
    u8 repeat_count;
    u8 _pad2;
    u16 timer;
    u16 frame_tile_count;
    u8 _pad3[0x30 - 0x2E];
    FieldImageReq upload;
    u16 scratch_pixels[257];
};

typedef struct
{
    u8 list_kind;
    u8 start_mask;
    u8 anim_index;
    u8 repeat_count;
    u8 stop_keyframe;
    u8 start_link;
    u8 end_link;
    u8 _pad7;
    u16 start_delay;
    u16 end_delay;
} FieldSeqDef;

typedef struct FieldSeq FieldSeq;

struct FieldSeq
{
    FieldSeq* next;
    FieldSeqDef* def;

    union
    {
        s32 word;

        struct
        {
            u8 state;
            u8 index;
            u8 _pad[2];
        } b;
    } flags;

    u16 phase_frames;
};

typedef struct
{
    u8 _pad0[0x20];

    struct
    {
        u16 x;
        u16 y;
        u16 w;
        u16 h;
    } rects[3];

    u8 _pad1[0x98 - 0x38];
    u8 chunk_idx;
    u8 _pad2[0x9D - 0x99];
    u8 frame_ready;
    u8 _pad3;
    u8 end_state;
} FieldMovieState;

typedef struct
{
    u16 count;
    u16 first;
} FieldNodeRun;

#define FIELD_NODE_RUN_COUNT_MASK 0x7FFF

#define FIELD_NODE_DEF_ENABLE_SHIFT 7

struct FieldNodeDef
{
    FieldNodeDef* next;
    s32 flags;
    u8 obj_index;
    u8 part_index;
    u16 vertex_c;
    u16 vertex_b;
    u16 vertex_a;
    s16 height0;
    s16 height1;
    s16 id_min;
    u8 _pad2[0x18 - 0x16];
    FieldNodeRun runs[1];
};

#define FIELD_NODE_DEF_ROWS(d) (((u8*)&(d)->flags)[2])

typedef struct FieldNode FieldNode;

struct FieldNode
{
    FieldNode* next;
    FieldNodeDef* def;
    FieldObj* obj;
    FieldPart* part;
    u16* spans;
    u8* span_flags;
    u8 active;
    u8 _pad2[0x1C - 0x19];
    s16 x_min;
    s16 x_max;
    s16 row_end;
    s16 row_start;
    s32 motion_x;
    s32 motion_height0;
    s32 motion_height1;
    s32 motion_z;
    s32 offset_x;
    s32 height0_offset;
    s32 height1_offset;
    s32 offset_z;
};

typedef union
{
    u32 word;

    struct
    {
        u8 flags;
        u8 r;
        u8 g;
        u8 b;
    } b;
} FieldMapColor;

typedef struct
{
    FieldObjDef** object_defs;
    u_long* image;
    FieldNodeDef* node_defs;
    FieldMarkerDef* edge_defs;
    FieldHeaderRec* records;
    FieldAnimDef* anim_defs[4];
    u8 _pad1[2];
    u16 built;
    u16 image_size;
    u16 pixel_count;
    FieldMapColor background;
    u16 width;
    u16 depth;
} FieldMapObject;

#if defined(LOM_NATIVE_POINTERS)
_Static_assert(sizeof(FieldSceneHeader) == sizeof(FieldMapObject), "FieldSceneHeader is FieldMapObject");
_Static_assert(offsetof(FieldSceneHeader, pixel_data) == offsetof(FieldMapObject, image), "FieldSceneHeader::pixel_data");
_Static_assert(offsetof(FieldSceneHeader, records) == offsetof(FieldMapObject, records), "FieldSceneHeader::records");
_Static_assert(offsetof(FieldSceneHeader, pixel_stride) == offsetof(FieldMapObject, image_size), "FieldSceneHeader::pixel_stride");
_Static_assert(offsetof(FieldSceneHeader, flags) == offsetof(FieldMapObject, background), "FieldSceneHeader::flags");
_Static_assert(offsetof(FieldSceneHeader, map_width) == offsetof(FieldMapObject, width), "FieldSceneHeader::map_width");
#endif

#if defined(LOM_NATIVE_POINTERS)
#define g_field_objects (*(FieldMapObject***)(FIELD_RESOURCE + 1))
#else
extern FieldMapObject** g_field_objects;
#endif

void field_build_render_records(FieldMapObject* object, u16 object_index);
void field_collision_rebuild_spans(void);

typedef struct
{
    u8 _pad0[8];
    u16 format_version;
    u8 _pad1[0x10 - 0xA];
    s32 seq_count;
    FieldScene* scene;
    FieldSeqDef* seq_defs;
    DVECTOR* points;
} FieldResource;

#if defined(LOM_NATIVE_POINTERS)
#include "lom_native_field_map.h"
#define FIELD_RESOURCE ((FieldResource*)lom_native_field_map_header)
#else
#define FIELD_RESOURCE ((FieldResource*)LOAD_BUFFER_ADDRESS)
#endif

#if defined(LOM_NATIVE_POINTERS)
#define g_field_mem_top (FIELD_WORKSPACE->top)
#else
extern u8* g_field_mem_top;
#endif
#if defined(LOM_NATIVE_POINTERS)
#define g_field_mem_base (FIELD_WORKSPACE->primitive_buffers[0])
#else
extern u8* g_field_mem_base;
#endif
#if defined(LOM_NATIVE_POINTERS)
#define g_field_mem_midpoint (FIELD_WORKSPACE->primitive_buffers[1])
#else
extern u8* g_field_mem_midpoint;
#endif
#if defined(LOM_NATIVE_POINTERS)
#define g_field_scene_fade_mode (FIELD_WORKSPACE->fade_mode)
#else
extern s32 g_field_scene_fade_mode;
#endif

extern s32 g_field_marker_overlay_enabled[2];
#if defined(LOM_NATIVE_POINTERS)
#define g_field_camera_x (SCENE_STATE->camera_x)
#else
extern s32 g_field_camera_x;
#endif
#if defined(LOM_NATIVE_POINTERS)
#define g_field_camera_y (SCENE_STATE->camera_y)
#else
extern s32 g_field_camera_y;
#endif
#if defined(LOM_NATIVE_POINTERS)
#define g_field_camera_z (SCENE_STATE->camera_z)
#else
extern s32 g_field_camera_z;
#endif
#if defined(LOM_NATIVE_POINTERS)
#define g_field_pixel_lookup_selector (*(s32*)&SCENE_STATE->pixel_lookup_selector)
#else
extern s32 g_field_pixel_lookup_selector;
#endif
#if defined(LOM_NATIVE_POINTERS)
#define g_field_node_angle_table (*(s16**)&FIELD_RESOURCE->points)
#else
extern s16* g_field_node_angle_table;
#endif

typedef struct
{
    s32 x;
    s32 y;
    s32 width;
    s32 camera_x;
    s32 camera_y;
} FieldViewport;

void field_start_sequence(FieldSeq* seq, u8 index);
void field_move_part_nodes(FieldPart* part, s32 delta, s32 axis);
void field_move_object_nodes(FieldObj* obj, s32 delta, s32 axis);
FieldObj* field_get_object(s32 index);
FieldPart* field_get_object_part(s32 obj_index, s32 part_index);
FieldPart* field_find_grid_part(FieldPartDef* grid, FieldTintSrc** out_src);
void field_build_tint_colors(u8* colors, s32 count, s32* rgb_scale);
void field_set_tint_primitive_code(u8 format, s32 count, u8* primitive_code);
FieldObj* field_find_object_by_definition(void* definition);

#if defined(LOM_NATIVE_POINTERS)
#define g_field_movie_frame_width (*(u16*)SECONDARY_OVERLAY_ADDRESS)
#define g_field_movie_frame_height (*(u16*)(SECONDARY_OVERLAY_ADDRESS + 2))
#else
extern u16 g_field_movie_frame_width;
extern u16 g_field_movie_frame_height;
#endif

typedef struct
{
    s16 x;
    s16 y;
    s16 z;
    u16 visibility;
} FieldTweenKey;

typedef struct FieldTweenSpan
{
    u8 count;
    u8 range_start;
    u16 duration;
} FieldTweenSpan;

#define FIELD_SPAN_COUNT_MASK 0x7F

typedef union
{
    u16 word;

    struct
    {
        u8 lo;
        u8 hi;
    } b;
} FieldSfxWord;

typedef struct
{
    FieldSfxWord control;
    FieldSfxWord sound;
    u16 sfx_id;
    u16 unused_6;
} FieldSfxKey;

#endif
