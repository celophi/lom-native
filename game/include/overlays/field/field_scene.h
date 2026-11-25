#ifndef FIELD_SCENE_H
#define FIELD_SCENE_H

#include "common.h"

typedef struct
{
    struct FieldSceneHeader* header;
    struct FieldObj* objects;
    struct FieldNode* nodes;
    struct FieldNode* sweep_node;
    struct FieldMarker* markers;
    struct FieldSeq* seqs;
    struct FieldAnim* anims;
    struct FieldAnim* strips;
    struct FieldAnim* sprites;
    struct FieldAnim* effects;
    u8* group_work;
    u8* group_tiles;
    u8* group_work_end;
    struct FieldImageReq* uploads;
    u8* vlc_table;
    s32 unused_3c;
    u8 tile_size;
    u8 group_count;
    s16 group_stride;
    u16 group_tile_count;
    u16 tile_cols;
    u16 tile_rows;
    s16 group_ids[10];
    s16 group_counters[10];
} FieldScene;

typedef struct
{
    FieldScene* scene;
} FieldSceneGlobals;

#if defined(LOM_NATIVE_POINTERS)
#define g_field_scene (*(FieldSceneGlobals*)&FIELD_RESOURCE->scene)
#else
extern FieldSceneGlobals g_field_scene;
#endif

#endif
