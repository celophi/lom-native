#include <libgpu.h>
#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_actor_palette.h"
#include "overlays/field/internal/field_mesh_render.h"
#include "main/cdrom.h"
#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include <stdio.h>
#include <string.h>
#include "../../support/lom_check.h"
extern void field_unpack_resource_package(void*, s32, s32, s32);

extern void field_load_actor_resource_slot(s32, s32, s32, s32);

extern s32 field_get_actor_resource_id(s32, FieldPlayerRecord*, s32);
#define WORD(a) (*(u32*)(uintptr_t)(a))
extern u8* g_field_resource_cursor;
extern u8 g_field_resource_buffer[];
extern u8* g_field_cd_buffer;
#define LoadImage __wrap_LoadImage
#define cdrom_queue_read __wrap_cdrom_queue_read
#define cdrom_wait_queue_empty __wrap_cdrom_wait_queue_empty
#define ALIAS_COUNT 1
#define BUFFER_BASE(alias) ((uintptr_t)g_field_resource_buffer)
#define CURSOR_SET(address) (g_field_resource_cursor = (u8*)(address))
#define CURSOR ((uintptr_t)g_field_resource_cursor)
#define RAM(address) ((u8*)MEMORY_MAP_RAM(address))
#define LOW_RAM(address) RAM(address)
#define IS_FILLED(pointer) ((void*)(pointer) == filled)
static RECT uploads[5];

static u_long* sources[5];

static unsigned count, reads, waits;

static u8* package;

static s32 package_size, read_resource;

static void* read_destination;

int LoadImage(RECT* rect, u_long* source)
{
    if (count < 5)
    {
        uploads[count] = *rect;
        sources[count] = source;
    }

    ++count;
    return 0;
}

s32 cdrom_queue_read(s32 resource, void* destination)
{
    ++reads;
    read_resource = resource;
    read_destination = destination;
    memcpy(destination, package, package_size);
    return package_size;
}

void cdrom_wait_queue_empty(void)
{
    ++waits;
}

/**
 * @brief Check a rectangle.
 * @param a The rectangle.
 * @param x Its left.
 * @param y Its top.
 * @param w Its width.
 * @param h Its height.
 * @return Nonzero when it's that one.
 */
static int rect_equal(RECT a, int x, int y, int w, int h)
{
    return a.x == x && a.y == y && a.w == w && a.h == h;
}

/**
 * @brief Check each party member's palette animation copies its colors to its
 *        own place in VRAM.
 * @return 0. A failed check stops the test.
 */
static int check_palette_animation(void)
{
    FieldActorSlot* actor = (void*)LOW_RAM(0x801E9000u);
    FieldAnimationDef* animation = (void*)LOW_RAM(0x801E9300u);
    memset(actor, 0, sizeof(*actor));
    memset(animation, 0, sizeof(*animation));
    actor->animation = animation;

    for (unsigned owner = 0; owner < 3; ++owner)
    {
        for (unsigned whole_row = 0; whole_row < 2; ++whole_row)
        {
            for (unsigned right = 0; right < 2; ++right)
            {
                u16* palette = (u16*)(owner < 2 ? g_field_actor_clut_buffers + owner * 0x400 : g_field_shared_clut_buffer);
                u16 before[512];

                for (unsigned i = 0; i < 512; ++i)
                {
                    palette[i] = before[i] = 0x1000 + i;
                }

                unsigned start = whole_row ? 0 : 80, colors = whole_row ? 256 : 16;
                actor->owner_object_index = owner;
                animation->palette_animation = (whole_row ? 0x33 : 0x523) | (right ? 0x80 : 0);
                actor->track_frames[0] = 2;
                count = 0;
                (void)field_update_actor_palette_animation(actor);
                CHECK(count == 0 && !memcmp(palette, before, sizeof(before)));
                actor->track_frames[0] = 3;
                (void)field_update_actor_palette_animation(actor);
                CHECK(count == 1 && rect_equal(uploads[0], start, owner < 2 ? 494 + owner * 2 : 498, colors, 1));
                CHECK(sources[0] == (u_long*)(palette + start));

                for (unsigned i = 0; i < 512; ++i)
                {
                    unsigned from = i;

                    if (i > start && i < start + colors)
                    {
                        from = right ? (i == start + 1 ? start + colors - 1 : i - 1) : (i == start + colors - 1 ? start + 1 : i + 1);
                    }

                    CHECK(palette[i] == before[from]);
                }

                animation->palette_animation &= ~0xF;
                (void)field_update_actor_palette_animation(actor);
                CHECK(count == 1);
            }
        }
    }

    return 0;
}

int main(void)
{
    CHECK(lom_native_map_ps1_ram() == 0);
    u32 words[160];
    const u32 first_texture[16] = {[2] = 32, [12] = (9u << 16) | 7};
    package = (u8*)words;

    for (unsigned i = 0; i < sizeof(words); ++i)
    {
        package[i] = i * 13 + 7;
    }

    for (unsigned texture = 0; texture < 3; ++texture)
    {
        u32* tim = words + 16 + texture * 32;
        tim[2] = 32;
        tim[12] = (9u << 16) | 7;
    }

    void* filled;
    memset(&filled, 0xCC, sizeof(filled));
    (void)filled;

    for (unsigned sections = 2; sections <= 4; ++sections)
    {
        for (unsigned slot = 0; slot < 4; ++slot)
        {
            for (unsigned alias = 0; alias < ALIAS_COUNT; ++alias)
            {
                words[0] = sections * 4;

                for (unsigned i = 1; i < sections - 1; ++i)
                {
                    words[i] = 64 + i * 128;
                }

                words[sections - 1] = 512;
                memcpy(package + words[0], first_texture, sizeof(first_texture));
                uintptr_t base = BUFFER_BASE(alias);
                CURSOR_SET(base);
                WORD(base + 8) = 0xA5A5A5A5u;
                memset(g_field_resource_entries, 0xCC, 9 * sizeof(FieldResourceEntry));
                count = 0;
                field_unpack_resource_package(package, 517, slot, 7);
                CHECK(CURSOR == base + 8 && WORD(base + 8) == 0xA5A5A5A5u);
                CHECK(!memcmp((void*)(uintptr_t)base, package + 512, 8));
                CHECK((uintptr_t)g_field_resource_entries[slot].start == base);
                CHECK(IS_FILLED(g_field_resource_entries[slot].end));
                CHECK(IS_FILLED(g_field_resource_entries[(slot + 1) % 9].start));
                CHECK(count == (sections == 2 ? 2 : sections == 3 ? 3 : 5));
                unsigned upload = 0;

                for (unsigned texture = 0; texture < sections - 1; ++texture)
                {
                    if (texture != 1)
                    {
                        CHECK(rect_equal(uploads[upload], texture == 2 ? 192 : 0, 507, texture == 2 ? 64 : 256, 1));
                        CHECK(sources[upload++] == (u_long*)(package + words[texture] + 20));
                    }

                    int x = slot >= 2 ? 832 - slot * 64 : (texture ? 960 : 896) - slot * 128;
                    int y = slot < 2 && texture == 2 ? 128 : 0;
                    int w = slot < 2 && texture == 1 ? 7 : 64;
                    int h = slot >= 2 || texture == 0 ? 256 : texture == 1 ? 9 : 128;
                    CHECK(rect_equal(uploads[upload], x, y, w, h));
                    CHECK(sources[upload++] == (u_long*)(package + words[texture] + 52));
                }
            }
        }
    }

    words[0] = 8;
    words[1] = 512;
    memcpy(package + 8, first_texture, sizeof(first_texture));
    memset(package + 512, 0, 68);
    package[514] = 32;
    package[516] = 1;
    package[518] = 16;
    package[528] = 1;
    package[529] = 0;
    package[530] = 3;
    package[546] = 48;
    package_size = 577;
    memset(RAM(0x80140000u), 0, 580);
    g_field_cd_buffer = RAM(0x80140000u);
    uintptr_t buffer = BUFFER_BASE(0);
    CURSOR_SET(buffer);
    WORD(buffer + 0x44) = 0xA5A5A5A5u;
    memset(&g_field_actors[0], 0, sizeof(FieldActor));
    memset(&g_field_resource_entries[0], 0, sizeof(FieldResourceEntry));
    count = 0;
    field_load_actor_resource_slot(0, 0, 0xAEB, 1);
    CHECK(reads == 1 && waits == 1 && read_resource == 0xAEB && read_destination == (void*)RAM(0x80140000u));
    CHECK(CURSOR == buffer + 0x44 && WORD(buffer + 0x44) == 0xA5A5A5A5u);
    CHECK((uintptr_t)g_field_resource_entries[0].start == buffer && (uintptr_t)g_field_resource_entries[0].end == buffer + 0x44);
    CHECK(g_field_resource_entries[0].flags == 3 && g_field_resource_entries[0].bound_animation_flags == 0x2F);
    CHECK((uintptr_t)g_field_actors[0].frame_data == buffer + 0x30 && g_field_actors[0].frame_timer == 1);
    CHECK(count == 2);

    FieldPlayerRecord player = {0};
    CHECK(field_get_actor_resource_id(0, &player, 0) == 0xAEB);
    player.head.bits.alt_appearance = 1;
    CHECK(field_get_actor_resource_id(0, &player, 0) == 0xAEC);
    player.head.bytes.weapon_type = 5;
    CHECK(field_get_actor_resource_id(0, &player, 1) == 0xA1C);
    player.head.bits.alt_appearance = 0;
    CHECK(field_get_actor_resource_id(0, &player, 1) == 0xA11);
    player.character_id = 3;
    player.character_kind = FIELD_PLAYER_KIND_PARTNER;
    CHECK(field_get_actor_resource_id(1, &player, 0) == 0xAF1 && field_get_actor_resource_id(1, &player, 1) == 0xA26);
    player.character_kind = FIELD_PLAYER_KIND_COMPANION;
    CHECK(field_get_actor_resource_id(2, &player, 0) == 0xB05 && field_get_actor_resource_id(2, &player, 1) == 0xA4E);
    CHECK(check_palette_animation() == 0);
    puts("LoM FIELD party smoke passed: package layouts, texture slots, both arena aliases, packed pointer neighbors, rounded copy, queued binding, resource "
         "selectors and private/shared palette cycles.");
    return 0;
}
