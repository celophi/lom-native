#include "common.h"
#include "sdk/libgpu.h"
#include "lom_native_ps1_memory.h"
#include <stdio.h>
#include <string.h>
#include "../../support/lom_check.h"

extern void field_preserve_scene_sections(u8*);

extern void field_copy_scene_geometry(s32*, s32*);

extern void field_upload_actor_image(void*, s32, s32, s32);

extern u8* field_get_event_script(s32);
#define WORD(addr) (*(u32*)(uintptr_t)(addr))
extern u8 g_field_scene_data_buffer[];
extern s32 g_field_scene_data_size;
extern u8* g_field_scene_strings;
extern u16* g_field_event_scripts;
extern u16* g_field_actor_scripts;
extern u32* g_field_scene_record_table;
extern u8* g_field_scene_portraits;
extern u32 g_field_scene_portrait_count;
extern u8* g_field_resource_cursor;
extern u8 g_field_resource_buffer[];
#define LoadImage __wrap_LoadImage
#define DrawSync __wrap_DrawSync
#define SCENE_FILE ((u8*)MEMORY_MAP_RAM(0x80190000u))
#define OUTPUT g_field_scene_data_buffer
#define AT(offset) ((uintptr_t)(g_field_scene_data_buffer + (offset)))
#define DATA_SIZE g_field_scene_data_size
#define STRINGS ((uintptr_t)g_field_scene_strings)
#define EVENT_SCRIPTS ((uintptr_t)g_field_event_scripts)
#define ACTOR_SCRIPTS ((uintptr_t)g_field_actor_scripts)
#define RECORD_TABLE ((uintptr_t)g_field_scene_record_table)
#define PORTRAITS ((uintptr_t)g_field_scene_portraits)
#define PORTRAIT_COUNT g_field_scene_portrait_count
#define RESOURCE_BUFFER g_field_resource_buffer
#define CURSOR_SET(pointer) (g_field_resource_cursor = (pointer))
#define CURSOR ((uintptr_t)g_field_resource_cursor)
static RECT uploads[2];

static u_long* sources[2];

static unsigned count, syncs;

int LoadImage(RECT* rect, u_long* source)
{
    if (count < 2)
    {
        uploads[count] = *rect;
        sources[count] = source;
    }

    ++count;
    return 0;
}

int DrawSync(int mode)
{
    ++syncs;
    return 0;
}

int main(void)
{
    CHECK(lom_native_map_ps1_ram() == 0);
    u8* scene = SCENE_FILE;
    u8* output = OUTPUT;
    u32* h = (u32*)scene;

    for (unsigned i = 0; i < 0x1000; ++i)
    {
        scene[i] = i * 13 + 7;
    }

    h[1] = 0x40;
    h[2] = 0x4C;
    h[3] = 0x54;
    h[4] = 0x64;
    h[5] = 0x68;
    h[8] = 0x100;
    *(u32*)(scene + 0x100) = 1;
    u8 expected[8 + 12 + 16 + 4 + 0x4A0];
    memcpy(expected, scene + 0x4C, 8);
    memcpy(expected + 8, scene + 0x40, 12);
    memcpy(expected + 20, scene + 0x54, 16);
    memcpy(expected + 36, scene + 0x64, 4);
    memcpy(expected + 40, scene + 0x104, 0x4A0);
    memset(output, 0xA5, sizeof(expected) + 4);
    field_preserve_scene_sections(scene);
    CHECK(!memcmp(output, expected, sizeof(expected)));
    CHECK(*(u32*)(output + sizeof(expected)) == 0xA5A5A5A5u);
    CHECK(DATA_SIZE == sizeof(expected));
    CHECK(STRINGS == AT(0));
    CHECK(EVENT_SCRIPTS == AT(8));
    CHECK(ACTOR_SCRIPTS == AT(0x14));
    CHECK(RECORD_TABLE == AT(0x24));
    CHECK(PORTRAITS == AT(0x28) && PORTRAIT_COUNT == 1);
    memset(scene, 0xCC, 0x1000);
    CHECK(!memcmp(output, expected, sizeof(expected)));
    u16* scripts = (u16*)(output + 8);
    scripts[0] = 4;
    scripts[1] = 7;
    CHECK((uintptr_t)field_get_event_script(0) == AT(0xC));
    CHECK((uintptr_t)field_get_event_script(1) == AT(0xF));

    for (unsigned i = 1; i <= 5; ++i)
    {
        h[i] = 0x40;
    }

    h[8] = 0x40;
    *(u32*)(scene + 0x40) = 0;
    field_preserve_scene_sections(scene);
    CHECK(DATA_SIZE == 0 && PORTRAIT_COUNT == 0);
    CHECK(EVENT_SCRIPTS == AT(0) && PORTRAITS == AT(0));

    u32 geometry[] = {0x12345678, 0x90ABCDEF, 0xA5A5A5A5};
    CURSOR_SET(RESOURCE_BUFFER);
    *(u32*)(RESOURCE_BUFFER + 8) = 0xDEADBEEFu;
    field_copy_scene_geometry((s32*)geometry, (s32*)((u8*)geometry + 5));
    CHECK(!memcmp(RESOURCE_BUFFER, geometry, 8));
    CHECK(CURSOR == (uintptr_t)(RESOURCE_BUFFER + 8) && *(u32*)(RESOURCE_BUFFER + 8) == 0xDEADBEEFu);
    field_copy_scene_geometry((s32*)geometry, (s32*)geometry);
    CHECK(CURSOR == (uintptr_t)(RESOURCE_BUFFER + 8));

    u32 tim[] = {0x10, 8, 20, 0, 0x00020002, 0x12345678, 0x90ABCDEF, 16, 0, 0x00010002, 0x13572468};
    field_upload_actor_image(tim, 3, 3, 1);
    CHECK(count == 2 && syncs == 1);
    CHECK(uploads[0].x == 0 && uploads[0].y == 503 && uploads[0].w == 4 && uploads[0].h == 1 && (void*)sources[0] == (void*)(tim + 5));
    CHECK(uploads[1].x == 640 && uploads[1].y == 0 && uploads[1].w == 2 && uploads[1].h == 1 && (void*)sources[1] == (void*)(tim + 10));
    count = 0;
    field_upload_actor_image(tim, 10, 3, 0);
    CHECK(count == 1 && syncs == 2);
    CHECK(uploads[0].x == 896 && uploads[0].y == 256 && (void*)sources[0] == (void*)(tim + 10));
    puts("LoM FIELD resource smoke passed: preservation, empty sections, packed slots, script lookup, rounded geometry, TIM destinations.");
    return 0;
}
