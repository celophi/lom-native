#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_calls.h"
#include "main/cdrom.h"
#include "lom_native_ps1_memory.h"
#include <stdio.h>
#include <string.h>
extern void field_reset_actor_resources(void);

extern s32 g_field_resource_queue_count, g_field_resource_queue[16], g_field_loading_resource;

extern s32 g_field_binding_restart_pending[3];
#define WORD(a) (*(u32*)(uintptr_t)(a))
#define HEAP_WORD(heap, offset) (*(u32*)((u8*)(heap) + (offset)))
#include "overlays/field/internal/field_mesh.h"
#include "../../support/lom_check.h"
#define ALIAS_COUNT 1
#define HEAP_AT(alias) ((u8*)MEMORY_MAP_RAM(0x80158000u))
#define SET_HEAP(heap) (g_field_actor_heap = (heap), g_field_cd_buffer = (heap) - 0x18000)
#define HEAP_IS(heap) (g_field_actor_heap == (heap) && g_field_cd_buffer == (heap) - 0x18000)
#define MESH_IS(heap)                                                                                                                                          \
    ((u8*)g_field_mesh_transformed_normals == (heap) + 4 && (u8*)g_field_mesh_screen_vertices == (heap) + 0x1808 &&                                            \
     (u8*)g_field_mesh_depth_offsets == (heap) + 0x240C)
#define GUARD_COUNT 0
static int accepted, reads;

static s32 resource;

static void* destination;

/**
 * @brief Note the read FIELD queues, and accept or refuse it as the case asks.
 * @param id The resource.
 * @param buffer Where it goes.
 * @return 32, or -1 when refused.
 */
s32 __wrap_cdrom_queue_read(s32 id, void* buffer)
{
    ++reads;
    resource = id;
    destination = buffer;
    return accepted ? 32 : -1;
}

/**
 * @brief Write a 32-bit word.
 * @param bytes The buffer.
 * @param offset Where in it.
 * @param value The word.
 */
static void put32(u8* bytes, unsigned offset, u32 value)
{
    memcpy(bytes + offset, &value, 4);
}

int main(void)
{
    CHECK(lom_native_map_ps1_ram() == 0);
    static u8 expected_heap[0x20000], expected_bindings[3 * sizeof(FieldActorBinding)];
    static const u32 guards[] = {0x8010578C, 0x80105794, 0x8010586C, 0x80105874, 0x8010587C, 0x801058D4, 0x8010A01C,
                                 0x8010A02C, 0x8010D02C, 0x8010D03C, 0x8010D084, 0x80157FFC, 0x80178000};

    (void)guards;

    for (unsigned alias = 0; alias < ALIAS_COUNT; ++alias)
    {
        u8* heap = HEAP_AT(alias);

        for (unsigned i = 0; i < GUARD_COUNT; ++i)
        {
            WORD(guards[i]) = 0xDEADBEEF;
        }

        memset(heap, 0xA5, 0x20000);
        memset(expected_heap, 0xA5, sizeof(expected_heap));
        memset(g_field_actor_bindings, 0xA5, sizeof(expected_bindings));
        memset(expected_bindings, 0xA5, sizeof(expected_bindings));

        for (unsigned i = 0; i < 3; ++i)
        {
            put32(expected_bindings, i * sizeof(FieldActorBinding) + offsetof(FieldActorBinding, state), FIELD_BINDING_IDLE);
            put32(expected_bindings, i * sizeof(FieldActorBinding) + offsetof(FieldActorBinding, load_id), 0);
            g_field_binding_restart_pending[i] = 9;
        }

        SET_HEAP(heap);
        g_field_resource_queue_count = 16;
        g_field_loading_resource = 123;
        memset(g_field_resource_queue, 0x6B, sizeof(g_field_resource_queue));
        field_reset_actor_resources();
        CHECK(!memcmp(g_field_actor_bindings, expected_bindings, sizeof(expected_bindings)));

        for (unsigned i = 0; i < 3; ++i)
        {
            CHECK(g_field_binding_restart_pending[i] == 0);
        }

        CHECK(g_field_resource_queue_count == 0 && g_field_loading_resource == 123);

        for (unsigned i = 0; i < 16; ++i)
        {
            CHECK(g_field_resource_queue[i] == 0x6B6B6B6B);
        }

        CHECK(HEAP_IS(heap));
        CHECK(MESH_IS(heap));
        put32(expected_heap, 0, 0x80401800);
        put32(expected_heap, 0x1804, 0x80400C00);
        put32(expected_heap, 0x2408, 0x80400C00);
        put32(expected_heap, 0x300C, 0x1CFEC);
        put32(expected_heap, 0x1FFFC, 0xFFF5A5A5);
        CHECK(!memcmp(heap, expected_heap, sizeof(expected_heap)));

        for (unsigned i = 0; i < GUARD_COUNT; ++i)
        {
            CHECK(WORD(guards[i]) == 0xDEADBEEF);
        }

        for (unsigned i = 0; i < 16; ++i)
        {
            CHECK(field_request_resource_read(0x500 + i) == 0);
        }

        CHECK(field_request_resource_read(0x999) == 1 && g_field_resource_queue_count == 16);
        accepted = 0;
        reads = 0;
        field_issue_next_resource_read();
        CHECK(reads == 1 && resource == 0x500 && destination == (void*)(heap - 0x18000));
        CHECK(g_field_resource_queue_count == 16 && field_get_loading_resource() == 123);
        accepted = 1;

        for (unsigned i = 0; i < 16; ++i)
        {
            field_issue_next_resource_read();
            CHECK(resource == 0x500 + i && field_get_loading_resource() == resource);
            CHECK(g_field_resource_queue_count == 15 - i);
        }

        field_issue_next_resource_read();
        CHECK(reads == 17 && field_get_loading_resource() == -1);
        CHECK(field_request_resource_read(7) == 0);
        field_clear_resource_queue();
        CHECK(g_field_resource_queue_count == 0 && g_field_resource_queue[0] == 7);
        void* owner = field_block_alloc(heap, 17, 2);
        CHECK(owner == heap + 0x3010);
        field_free_owner_resources(2);
        CHECK(HEAP_WORD(heap, 0x300C) == 0x1CFEC && HEAP_WORD(heap, 0) == 0x80401800);
        void* whole = field_block_alloc(heap, 0x1CFEC, 1);
        CHECK(whole == owner && field_block_alloc(heap, 4, 0) == NULL);
        field_free_owner_resources(1);
        CHECK(field_block_alloc(heap, 0x1CFE8, 1) == owner);
        CHECK((HEAP_WORD(heap, 0x300C) & 0xFFFFF) == 0x1CFEC);
        field_free_owner_resources(1);
    }

    u32 pool[32];
    memset(pool, 0xA5, sizeof(pool));
    field_block_pool_init(pool, sizeof(pool));
    void* a = field_block_alloc(pool, 13, 1);
    void* b = field_block_alloc(pool, 20, 2);
    void* c = field_block_alloc(pool, 13, 1);
    CHECK(a == pool + 1 && b == pool + 6 && c == pool + 12);
    field_block_free_tag(pool, 1);
    CHECK(field_block_alloc(pool, 60, 3) == c);
    field_block_free_tag(pool, 3);
    field_block_free_tag(pool, 2);
    CHECK(pool[0] == 120 && field_block_alloc(pool, 120, 3) == a);
    puts("LoM FIELD heap smoke passed: packed reset/both aliases, guarded shared buffers, tagged allocation/release, FIFO/backpressure/exhaustion.");
    return 0;
}
