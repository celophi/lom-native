#ifndef FIELD_MESH_H
#define FIELD_MESH_H

#include "common.h"
#include <libgte.h>

typedef struct FieldMeshTexturePart
{
    u8 x;
    u8 y;
    u8 width;
    u8 height;
    u16* pixels;
} FieldMeshTexturePart;

typedef struct FieldMeshFace
{
    union
    {
        u16 uv[3];
        u8 color[3];
    } texture;

    u8 kind;
    u8 vertex_colors[3][3];
} FieldMeshFace;

#define FIELD_MESH_FACE_KIND(face) (((face)->kind >> 1) & 0xF)

typedef struct FieldMeshResource
{
    u16 face_count;
    u8 texture_part_count;
    u8 pad3;
    FieldMeshTexturePart* texture_parts;
    SVECTOR* vertices;
    SVECTOR* normals;
    SVECTOR* offsets;
    FieldMeshFace* faces;
} FieldMeshResource;

#define FIELD_ACTOR_MESH(actor, index) (&(actor)->meshes[index])

#define FIELD_MESH_VERTEX_MAX 768

extern s16* g_field_mesh_screen_vertices;
extern SVECTOR* g_field_mesh_transformed_normals;
extern s32* g_field_mesh_depth_offsets;

#endif
