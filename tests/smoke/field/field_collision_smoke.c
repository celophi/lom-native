#include "overlays/field/internal/field_scene_internal.h"
#include "lom_native_ps1_memory.h"
#include "overlays/field/internal/field_calls.h"
#define RAM(address) ((u8*)MEMORY_MAP_RAM(address))
#define SET_SCENE(new_scene, new_points) (FIELD_RESOURCE->scene = (new_scene), FIELD_RESOURCE->points = (DVECTOR*)(new_points))
#define WORK_TOP_SET(address) (g_field_mem_top = (address))
#define WORK_TOP() ((uintptr_t)g_field_mem_top)
#include <stdio.h>
#include <string.h>
#include "../../support/lom_check.h"

extern void field_collision_rebuild_spans(void);

extern void* field_header_record_at(s32);

int main(void)
{
    CHECK(lom_native_map_ps1_ram() == 0);
    FieldScene* scene = (void*)RAM(0x80182000u);
    FieldSceneHeader header = {.map_width = 64, .map_depth = 64};
    FieldNode nodes[2] = {0};

    struct
    {
        FieldNodeDef def;

        FieldNodeRun end;
    } defs[2] = {0};

    s16 points[] = {8, 8, 19, 8, 19, 23, 8, 23, 8, 16, 32, 32, 43, 32, 43, 47, 32, 47, 32, 40};
    memcpy(RAM(0x80181000u), points, sizeof(points));
    SET_SCENE(scene, RAM(0x80181000u));
    scene->header = &header;
    scene->nodes = &nodes[1];
    nodes[1].next = &nodes[0];

    for (unsigned n = 0; n < 2; ++n)
    {
        defs[n].def.flags = 0x10004;
        defs[n].def.runs[0].count = 5;
        defs[n].def.runs[0].first = n * 5;
        nodes[n].def = &defs[n].def;
        nodes[n].x_min = 8 + n * 24;
        nodes[n].x_max = 19 + n * 24;
        nodes[n].row_start = 8 + n * 24;
        nodes[n].row_end = 23 + n * 24;
    }

    u32 masks[4][16];

    for (unsigned active = 0; active < 4; ++active)
    {
        nodes[0].active = active & 1;
        nodes[1].active = (active >> 1) & 1;
        WORK_TOP_SET(RAM(0x80183000u));
        memset(RAM(0x80183000u), 0xA5, 0x1000);
        field_collision_rebuild_spans();
        CHECK(scene->group_count == 1 && scene->group_ids[0] == 0);
        CHECK(scene->tile_size == 4 && scene->tile_cols == 20 && scene->tile_rows == 12);
        CHECK(scene->group_work_end - scene->group_work == sizeof(masks[0]));
        CHECK(WORK_TOP() == (uintptr_t)scene->group_work_end);
        CHECK(*(u32*)scene->group_work_end == 0xA5A5A5A5u);

        for (unsigned n = 0; n < 2; ++n)
        {
            CHECK(nodes[n].spans && nodes[n].span_flags);

            for (unsigned row = 0; row < 16; ++row)
            {
                CHECK(nodes[n].spans[row * 2] == nodes[n].x_min);
                CHECK(nodes[n].spans[row * 2 + 1] == nodes[n].x_max);
            }
        }

        memcpy(masks[active], scene->group_work, sizeof(masks[0]));
    }

    unsigned first = 0, second = 0;

    for (unsigned word = 0; word < 16; ++word)
    {
        const u32 border = 3u | (3u << 18);
        const unsigned row = word / 2;
        CHECK(masks[0][word] == border);
        CHECK(masks[1][word] == (border | (row == 1 || row == 2 ? 0x70u : 0)));
        CHECK(masks[2][word] == (border | (row == 4 || row == 5 ? 0x1C00u : 0)));
        CHECK((masks[1][word] & masks[2][word]) == border);
        CHECK(masks[3][word] == (masks[1][word] | masks[2][word]));
        first += masks[1][word] != border;
        second += masks[2][word] != border;
    }

    CHECK(first == 4 && second == 4);
    scene->nodes = NULL;
    field_collision_rebuild_spans();
    CHECK(scene->group_count == 0 && scene->group_work == 0);

    static FieldNode inactive;
    static FieldNodeDef inactive_def;
    inactive.def = &inactive_def;
    scene->nodes = &inactive;

    FieldCollisionMover mover;

    for (unsigned sentinel = 0; sentinel < 2; ++sentinel)
    {
        memset(&mover, 0, sizeof(mover));
        mover.x = mover.z = 32 * 256;
        mover.move_x = 256;
        mover.footprint_width = 2;
        mover.mode.mode_flags = 2;
        mover.height_bias = 2;
        mover.collision_node = sentinel ? FIELD_COLLISION_NODE_SEARCH : FIELD_COLLISION_NODE_DETACHED;
        CHECK(field_collision_resolve_move(&mover) == 0);
        CHECK(mover.x == 33 * 256 && mover.z == 32 * 256);
    }

    scene->nodes = NULL;
    static FieldHeaderRec chain[2];
    chain[0].next = &chain[1];
    chain[1].next = NULL;
    header.records = chain;
    CHECK(field_header_record_at(0) == (void*)&chain[0].body);
    CHECK(field_header_record_at(1) == (void*)&chain[1].body);
    CHECK(field_header_record_at(2) == field_header_record_at(1));
    header.records = 0;
    CHECK(field_header_record_at(0) == NULL);
    puts("LoM FIELD collision smoke passed: rectangle spans, both mask planes, inactive/empty nodes, packed floor sentinels and records.");
    return 0;
}
