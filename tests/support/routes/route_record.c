#include "overlays/field/internal/field_actor.h"
#include "overlays/field/internal/field_records.h"
#include "common/saved_game.h"
#include "route_record.h"
#include "route_record_internal.h"
#include "lom_native_boundary.h"
#include "lom_native_draw_labels.h"
#include "lom_native_unit_decode.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    PS1_RAM_BYTES = 0x200000,
    SCRIPT_VARIABLE_WORDS = 0x100,
    VARIABLE_WORDS_PER_LINE = 16,
};

static const char RAM_HASH_VARIABLE[] = "LOM_ROUTE_RAM_HASH";

static const char SAVED_GAME_DUMP_VARIABLE[] = "LOM_ROUTE_SAVED_GAME_DUMP";

static const char RAM_DUMP_VARIABLE[] = "LOM_ROUTE_RAM_DUMP";

static const char CAPTURE_VARIABLE[] = "LOM_ROUTE_CAPTURE";

static const char CAPTURE_EVERY_VARIABLE[] = "LOM_ROUTE_CAPTURE_EVERY";

static FILE* s_frames;

static char s_directory[512];

static int s_saves;

static int s_ram_dumps;

static int s_captures;

static unsigned s_frames_recorded;

static FILE* s_boundary;
static int s_boundary_log;
static unsigned long long s_boundary_count[LOM_BOUNDARY_COUNT];
static unsigned long long s_boundary_hash[LOM_BOUNDARY_COUNT];

/**
 * @brief Count and hash each boundary event, and log it when
 *        LOM_ROUTE_BOUNDARY_LOG is set.
 * @param boundary Which boundary.
 * @param what Which call.
 * @param a Its first argument.
 * @param b Its second.
 */
static void boundary_observer(LomBoundary boundary, unsigned what, unsigned a, unsigned b)
{
    if ((unsigned)boundary >= LOM_BOUNDARY_COUNT)
    {
        return;
    }

    const unsigned words[3] = {what, a, b};
    const unsigned char* bytes = (const unsigned char*)words;
    unsigned long long value = s_boundary_hash[boundary];

    for (unsigned i = 0; i < sizeof(words); i++)
    {
        value = (value ^ bytes[i]) * ROUTE_FNV_PRIME;
    }

    s_boundary_hash[boundary] = value;
    s_boundary_count[boundary]++;

    if (s_boundary_log && s_boundary)
    {
        fprintf(s_boundary, "event frame %u %s %u %u %u\n", s_frames_recorded, lom_native_boundary_name(boundary), what, a, b);
    }
}

void route_record_mix(unsigned long long* hash, const void* data, unsigned long size)
{
    const unsigned char* bytes = data;

    for (unsigned long i = 0; i < size; i++)
    {
        *hash = (*hash ^ bytes[i]) * ROUTE_FNV_PRIME;
    }
}

typedef struct
{
    int index, presence, x, y, z, direction, animation, animation_frame, control, command;
} ActorState;

typedef struct
{
    int index, side, monster, hp, hp_max;
} BattleState;

/**
 * @brief Read one actor's state.
 * @param index Actor slot.
 * @param out Receives the state.
 * @return Non-zero when the slot is in use.
 */
static int actor_state(int index, ActorState* out)
{
    const FieldActor* actor = &g_field_actors[index];

    if (actor->presence == FIELD_ACTOR_UNUSED)
    {
        return 0;
    }

    *out = (ActorState){index,
                        actor->presence,
                        actor->x,
                        actor->y,
                        actor->z,
                        actor->direction,
                        actor->animation,
                        actor->animation_frame,
                        (int)(actor->control.word & FIELD_CONTROL_MODE_MASK),
                        actor->command};
    return 1;
}

/**
 * @brief Read one battle record's state.
 * @param index Record index.
 * @param out Receives the state.
 * @return Non-zero when there's a battle and the record is in use.
 */
static int battle_state(int index, BattleState* out)
{
    const FieldStatusRecord* record;

    if (g_field_battle == NULL)
    {
        return 0;
    }

    record = &g_field_battle->records[index];

    if (record->state == NULL || record->state == FIELD_STATUS_STATE_NONE)
    {
        return 0;
    }

    *out = (BattleState){index, record->side, record->template ? record->template->id : -1, record->state->current_hp.word, (int)record->state->maximum_hp};
    return 1;
}

int route_field_running(void)
{
    if (!lom_unit_is_loaded("field"))
    {
        return 0;
    }
    return g_field_runtime != NULL;
}

/**

 * @brief Hash the decoded game state.
 * @return The hash; zero outside a scene.
 */
static unsigned long long game_state_hash(void)
{
    unsigned long long result = ROUTE_FNV_OFFSET;
    ActorState actor;
    BattleState battle;

    if (!route_field_running())
    {
        return 0;
    }

    route_record_mix(&result, g_field_runtime, SCRIPT_VARIABLE_WORDS * sizeof(u32));

    for (int i = 0; i < FIELD_ACTOR_COUNT; i++)
    {
        if (actor_state(i, &actor))
        {
            route_record_mix(&result, &actor, sizeof(actor));
        }
    }

    for (int i = 0; i < FIELD_BATTLE_RECORD_COUNT; i++)
    {
        if (battle_state(i, &battle))
        {
            route_record_mix(&result, &battle, sizeof(battle));
        }
    }

    return result;
}

/**
 * @brief Write the decoded game state as readable lines, for explaining
 *        differences.
 * @param name The checkpoint's name.
 */
static void write_game_state(const char* name)
{
    ActorState actor;
    BattleState battle;
    const u32* words = (const u32*)g_field_runtime;

    if (!route_field_running())
    {
        return;
    }

    for (int i = 0; i < SCRIPT_VARIABLE_WORDS; i += VARIABLE_WORDS_PER_LINE)
    {
        fprintf(s_frames, "state %s variables %d", name, i);

        for (int j = 0; j < VARIABLE_WORDS_PER_LINE; j++)
        {
            fprintf(s_frames, " %08x", words[i + j]);
        }

        fprintf(s_frames, "\n");
    }

    for (int i = 0; i < FIELD_ACTOR_COUNT; i++)
    {
        if (actor_state(i, &actor))
        {
            fprintf(s_frames, "state %s actor %d presence %d position %d,%d,%d direction %d animation %d,%d control %d command %d\n", name, actor.index,
                    actor.presence, actor.x, actor.y, actor.z, actor.direction, actor.animation, actor.animation_frame, actor.control, actor.command);
        }
    }

    for (int i = 0; i < FIELD_BATTLE_RECORD_COUNT; i++)
    {
        if (battle_state(i, &battle))
        {
            fprintf(s_frames, "state %s battle %d side %d monster %d hp %d/%d\n", name, battle.index, battle.side, battle.monster, battle.hp, battle.hp_max);
        }
    }
}

unsigned long long route_record_hash(const void* data, unsigned long size)
{
    unsigned long long result = ROUTE_FNV_OFFSET;

    route_record_mix(&result, data, size);
    return result;
}

/**
 * @brief Write bytes to a file in the record folder. A file that can't be
 *        written is left out.
 * @param name File name, a printf format with one int.
 * @param number The number for the name.
 * @param data Bytes to write.
 * @param size Number of bytes.
 */
static void write_record_file(const char* name, int number, const void* data, unsigned long size)
{
    char path[ROUTE_RECORD_PATH_BYTES];
    char file_name[64];
    FILE* file;

    snprintf(file_name, sizeof(file_name), name, number);
    snprintf(path, sizeof(path), "%s/%s", s_directory, file_name);

    if ((file = fopen(path, "wb")))
    {
        fwrite(data, 1, size, file);
        fclose(file);
    }
}

extern unsigned char* Psyz_VideoAllocCapturedFrame(int* w, int* h);

/**
 * @brief Save the frame on screen as a PPM in the route's folder.
 * @param name The file's name.
 */
static void write_picture(const char* name)
{
    char path[ROUTE_RECORD_PATH_BYTES];
    int w = 0, h = 0;
    unsigned char* pixels = Psyz_VideoAllocCapturedFrame(&w, &h);
    FILE* file;

    snprintf(path, sizeof(path), "%s/%s", s_directory, name);

    if (pixels && w > 0 && h > 0 && (file = fopen(path, "wb")))
    {
        fprintf(file, "P6 %d %d 255\n", w, h);
        fwrite(pixels, 3, (size_t)w * (size_t)h, file);
        fclose(file);
    }

    free(pixels);
}

/**
 * @brief Save the frame on screen as the next capture-N.ppm.
 */
static void write_capture(void)
{
    char name[32];
    snprintf(name, sizeof(name), "capture-%d.ppm", s_captures++);
    write_picture(name);
}

const char* route_record_directory(void)
{
    return s_directory;
}

int route_record_recording(void)
{
    return s_frames != NULL;
}

int route_record_open(const char* directory)
{
    char path[ROUTE_RECORD_PATH_BYTES];

    snprintf(s_directory, sizeof(s_directory), "%s", directory);

    snprintf(path, sizeof(path), "%s/frames.txt", directory);
    s_frames = fopen(path, "w");

    snprintf(path, sizeof(path), "%s/boundary.txt", directory);
    s_boundary = fopen(path, "w");
    s_boundary_log = getenv("LOM_ROUTE_BOUNDARY_LOG") != NULL;

    for (int i = 0; i < LOM_BOUNDARY_COUNT; i++)
    {
        s_boundary_hash[i] = ROUTE_FNV_OFFSET;
        s_boundary_count[i] = 0;
    }

    lom_native_boundary_set_observer(boundary_observer);

    route_record_gpu_open();
    return s_frames ? 0 : -1;
}

void route_record_frame(unsigned frame, int scene, int hero_x, int hero_z)
{
    if (!s_frames)
    {
        return;
    }

    fprintf(s_frames, "frame %u scene %d hero %d,%d save %016llx state %016llx picture %016llx sound %016llx\n", frame, scene, hero_x, hero_z,
            route_record_hash(&g_saved_game, SAVED_GAME_DATA_SIZE), game_state_hash(), route_record_picture(), route_record_sound());
    route_record_gpu_frame_start();

    const char* every = getenv(CAPTURE_EVERY_VARIABLE);
    const long step = every ? strtol(every, NULL, 10) : 0;
    if (step > 0 && s_frames_recorded % (unsigned long)step == 0)
    {
        char name[32];
        snprintf(name, sizeof(name), "frame-%06u.ppm", s_frames_recorded);
        write_picture(name);
    }
    s_frames_recorded++;

    route_record_gpu_frame_end(frame);
}

void route_record_screen(unsigned vblank)
{
    const char* every = getenv(CAPTURE_EVERY_VARIABLE);
    const long step = every ? strtol(every, NULL, 10) : 0;

    if (s_frames && step > 0 && vblank % (unsigned long)step == 0)
    {
        char name[32];
        snprintf(name, sizeof(name), "screen-%06u.ppm", vblank);
        write_picture(name);
    }
}

void route_record_checkpoint(const char* name, unsigned frame, int scene)
{
    unsigned long long ram = 0;

    if (!s_frames)
    {
        return;
    }

    extern unsigned char lom_native_ram[PS1_RAM_BYTES * MEMORY_MAP_SCALE];
    const void* ram_bytes = lom_native_ram;
    unsigned long ram_size = sizeof(lom_native_ram);

    if (getenv(RAM_HASH_VARIABLE))
    {
        ram = route_record_hash(ram_bytes, ram_size);
    }

    if (getenv(RAM_DUMP_VARIABLE))
    {
        write_record_file("ram-%d.bin", s_ram_dumps++, ram_bytes, ram_size);
    }

    write_record_file("save-%d.bin", s_saves++, &g_saved_game, SAVED_GAME_DATA_SIZE);

    fprintf(s_frames, "checkpoint %s frame %u scene %d save %016llx state %016llx ram %016llx\n", name, frame, scene,
            route_record_hash(&g_saved_game, SAVED_GAME_DATA_SIZE), game_state_hash(), ram);

    if (getenv(CAPTURE_VARIABLE))
    {
        write_capture();
    }
    write_game_state(name);

    if (s_boundary)
    {
        for (int i = 0; i < LOM_BOUNDARY_COUNT; i++)
        {
            if (s_boundary_count[i])
            {
                fprintf(s_boundary, "checkpoint %s %s %llu %016llx\n", name, lom_native_boundary_name((LomBoundary)i), s_boundary_count[i], s_boundary_hash[i]);
            }
        }
    }
}

void route_record_note(const char* text)
{
    if (s_frames)
    {
        fprintf(s_frames, "note %s\n", text);
    }
}

void route_record_close(int steps_done, int steps_total, int failed)
{
    char path[ROUTE_RECORD_PATH_BYTES];
    FILE* cover;

    if (!s_frames)
    {
        return;
    }

    fprintf(s_frames, "end steps %d of %d failed %d cover_dropped %u\n", steps_done, steps_total, failed, route_record_cover_dropped());

    if (ferror(s_frames) | fclose(s_frames))
    {
        fprintf(stderr, "route-record: writing %s/frames.txt failed; the record is incomplete\n", s_directory);
    }

    s_frames = NULL;
    lom_native_boundary_set_observer(NULL);
    lom_native_draw_labels_report();

    if (s_boundary)
    {
        for (int i = 0; i < LOM_BOUNDARY_COUNT; i++)
        {
            if (s_boundary_count[i])
            {
                fprintf(s_boundary, "end %s %llu %016llx\n", lom_native_boundary_name((LomBoundary)i), s_boundary_count[i], s_boundary_hash[i]);
            }
        }

        fclose(s_boundary);
        s_boundary = NULL;
    }

    if (getenv(SAVED_GAME_DUMP_VARIABLE) && !failed)
    {
        write_record_file("saved-game.bin", 0, &g_saved_game, SAVED_GAME_DATA_SIZE);
    }

    snprintf(path, sizeof(path), "%s/coverage.txt", s_directory);
    cover = fopen(path, "w");

    if (cover)
    {
        route_record_write_cover(cover);
        fclose(cover);
    }
}
