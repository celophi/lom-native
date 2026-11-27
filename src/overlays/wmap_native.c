#include "common.h"
#include "lom_native_game_loop.h"
#include "lom_native_draw_labels.h"
#include "lom_native_game.h"
#include "lom_native_psyz.h"
#include "main/main_steps.h"
#include <libgte.h>
#include <libgpu.h>
#include <psyz/video.h>
#include "overlays/wmap/internal/wmap_frame_render.h"
#include "overlays/wmap/internal/wmap_map_display.h"
#include "overlays/wmap/internal/wmap_sequence_runtime.h"
#include "overlays/wmap/world_map.h"
#include "main/cdrom.h"
#include "main/cd_resources.h"
#include "main/screen_transition.h"
#include "main/overlay_memory.h"
#include "lom_native_wmap.h"
#include "lom_native_wmap_math.h"
#include "lom_native_gpu.h"
#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_setjmp.h"
#include "lom_native_widescreen.h"
#include "lom_native_gpu_packets.h"
#include <stdio.h>
#include <stdlib.h>

_Static_assert(GPU_PS1_PACKET_SIZE(SPRT) == 20 && GPU_PS1_PACKET_SIZE(POLY_FT4) == 40, "WMAP packet budget");

/**
 * @brief Psy-Q's gte_ldv0: load a short vector into the GTE's V0.
 * @param v The vector.
 */
static void load_vector(const SVECTOR* v)
{
    Psyz_GteDataWrite(0, (unsigned)(u16)v->vx | (unsigned)v->vy << 16);
    Psyz_GteDataWrite(1, (unsigned)(u16)v->vz);
}

#undef gte_ldv0
#undef gte_stlvnl
#define gte_ldv0(v) load_vector(v)
#define gte_stlvnl(p) ((p)->vx = (s32)Psyz_GteDataRead(25), (p)->vy = (s32)Psyz_GteDataRead(26), (p)->vz = (s32)Psyz_GteDataRead(27))

#define WMAP_CLIP_LEFT 8
#define WMAP_MESH_TRIANGLES 176
#define WMAP_MESH_POINTS 3
#define WMAP_MESH_SIDE_POINTS 8
#define WMAP_MESH_OT_INDEX 9
#define WMAP_MESH_TPAGE 0x7B54
#define WMAP_MESH_TPAGE_X 400

extern POLY_G3 g_wmap_transition_mesh_triangles_0[WMAP_MESH_TRIANGLES];
extern POLY_G3 g_wmap_transition_mesh_triangles_1[WMAP_MESH_TRIANGLES];

typedef struct
{
    s16 x, y;
} WmapMeshPoint;

extern WmapMeshPoint g_wmap_transition_mesh_vertices[WMAP_MESH_TRIANGLES * WMAP_MESH_POINTS];

typedef struct
{
    s16 rest_y;
    s16 x, y;
    u8 r, g, b;
} MeshSidePoint;

static POLY_G4 s_mesh_sides[2][2][WMAP_MESH_SIDE_POINTS];
static POLY_FT3 s_mesh_blend[2];

static LomJumpBuffer s_checkpoint;

static int s_active;

/**
 * @brief The quit hook while the world map runs: unwind out of it.
 */
static void wmap_quit(void)
{
    LOM_LONGJMP(s_checkpoint, 3);
}

/**
 * @brief Stop the world map because something it did failed. Outside a guarded
 *        run, abort.
 * @param operation What failed, for the message.
 */
static void fail_checkpoint(const char* operation)
{
    fprintf(stderr, "[lom-native] WMAP %s failed.\n", operation);

    if (s_active)
    {
        LOM_LONGJMP(s_checkpoint, 2);
    }

    abort();
}

/**
 * @brief WMAP's ClearOTagR, at the start of each of its frames: start the
 *        frame wide when Widescreen is on, as the Remaster draws the world
 *        map, with its corners where the PS1 has them and the map reaching
 *        the screen's sides. The PS1 keeps an 8-pixel strip at the left
 *        clear; a wide frame draws from the edge, so the renderer carries the
 *        drawing on into the sides.
 * @param ot The ordering table.
 * @param count Its length.
 * @return The ordering table.
 */
void* lom_native_wmap_ClearOTagR(void* ot, int count)
{
    ClearOTagR(ot, count);
    const int margin = lom_native_widescreen_begin_frame(lom_native_widescreen_enabled());

    for (int i = 0; i < 2; ++i)
    {
        g_wmap_frames[i].draw_env.clip.x = margin ? 0 : WMAP_CLIP_LEFT;
        g_wmap_frames[i].draw_env.clip.w = LOM_WIDESCREEN_ORIGINAL_WIDTH - (margin ? 0 : WMAP_CLIP_LEFT);
    }

    return ot;
}

/**
 * @brief Find the transition mesh's points on one of the screen's sides, as
 *        they are now, top to bottom. Which points those are is decided from
 *        where they rest, since a transition moves every point. Each triangle
 *        keeps its own copy of a point; the first copy found stands for it.
 * @param mesh The triangles drawn this frame.
 * @param side -1 for the left side, 1 for the right.
 * @param points Set to the points.
 * @return How many there are.
 */
static int mesh_side_points(const POLY_G3* mesh, int side, MeshSidePoint points[WMAP_MESH_SIDE_POINTS])
{
    int count = 0;

    for (int i = 0; i < WMAP_MESH_TRIANGLES * WMAP_MESH_POINTS; ++i)
    {
        const WmapMeshPoint* rest = &g_wmap_transition_mesh_vertices[i];
        const int on_side = side < 0 ? rest->x <= WMAP_CLIP_LEFT : rest->x >= LOM_WIDESCREEN_ORIGINAL_WIDTH - WMAP_CLIP_LEFT;
        int known = 0;

        for (int k = 0; k < count && !known; ++k)
        {
            known = points[k].rest_y == rest->y;
        }

        if (!on_side || known || count == WMAP_MESH_SIDE_POINTS)
        {
            continue;
        }

        const POLY_G3* triangle = &mesh[i / WMAP_MESH_POINTS];
        const int p = i % WMAP_MESH_POINTS;
        MeshSidePoint* point = &points[count++];
        point->rest_y = rest->y;
        point->x = p == 0 ? triangle->x0 : p == 1 ? triangle->x1 : triangle->x2;
        point->y = p == 0 ? triangle->y0 : p == 1 ? triangle->y1 : triangle->y2;
        point->r = p == 0 ? triangle->r0 : p == 1 ? triangle->r1 : triangle->r2;
        point->g = p == 0 ? triangle->g0 : p == 1 ? triangle->g1 : triangle->g2;
        point->b = p == 0 ? triangle->b0 : p == 1 ? triangle->b1 : triangle->b2;
    }

    for (int a = 1; a < count; ++a)
    {
        for (int b = a; b > 0 && points[b - 1].rest_y > points[b].rest_y; --b)
        {
            const MeshSidePoint swap = points[b];
            points[b] = points[b - 1];
            points[b - 1] = swap;
        }
    }

    return count;
}

/**
 * @brief In a wide frame, carry the transition mesh, the shading WMAP lays
 *        over the whole screen and moves for its transitions, on across the
 *        frame's sides. The mesh itself stays as it is: moving its edge
 *        points out that far would turn triangles whose other points a
 *        transition has moved over onto their neighbors, darkening the
 *        overlap. Instead each stretch of its edge between two edge points
 *        gets a shaded quad from there out past the frame's side, in the edge
 *        points' colors, blended the way the mesh is.
 * @param frame Which of the two frames this is, for its own packets.
 */
static void extend_mesh(int frame)
{
    const int margin = lom_native_widescreen_margin();

    if (!margin || g_wmap_transition_mesh_hidden == 1)
    {
        return;
    }

    const POLY_G3* mesh = g_wmap_frame_count & 1 ? g_wmap_transition_mesh_triangles_0 : g_wmap_transition_mesh_triangles_1;
    OT_TYPE* slot = &g_wmap_current_frame->ordering_table[WMAP_MESH_OT_INDEX];

    for (int side = -1; side <= 1; side += 2)
    {
        MeshSidePoint points[WMAP_MESH_SIDE_POINTS];
        const int count = mesh_side_points(mesh, side, points);
        const int out = side * (margin + WMAP_CLIP_LEFT * 2);

        for (int i = 0; i + 1 < count; ++i)
        {
            POLY_G4* quad = &s_mesh_sides[frame][side > 0][i];
            const MeshSidePoint* top = &points[i];
            const MeshSidePoint* bottom = &points[i + 1];
            lom_gpu_init_poly_g4(quad);
            lom_gpu_set_semi_trans(quad, 1);
            setXY4(quad, top->x + out, top->y, top->x, top->y, bottom->x + out, bottom->y, bottom->x, bottom->y);
            setRGB0(quad, top->r, top->g, top->b);
            setRGB1(quad, top->r, top->g, top->b);
            setRGB2(quad, bottom->r, bottom->g, bottom->b);
            setRGB3(quad, bottom->r, bottom->g, bottom->b);
            lom_gpu_link(slot, quad);
        }
    }

    POLY_FT3* blend = &s_mesh_blend[frame];
    lom_gpu_init_poly_ft3(blend);
    blend->tpage = WMAP_MESH_TPAGE;
    setXY3(blend, WMAP_MESH_TPAGE_X, 0, WMAP_MESH_TPAGE_X, 0, WMAP_MESH_TPAGE_X, 0);
    lom_gpu_link(slot, blend);
}

void lom_native_wmap_DrawOTag(const void* ot)
{
    WmapFrame* frame = (WmapFrame*)(uintptr_t)g_wmap_current_frame;
    unsigned index;

    if (frame == &g_wmap_frames[0] || frame == &g_wmap_frames[1])
    {
        index = frame == &g_wmap_frames[1];
    }
    else
    {
        fail_checkpoint("frame pointer");
        return;
    }

    u8* packets = index ? g_wmap_packet_buffer_1 : g_wmap_packet_buffer_0;

    if (g_wmap_packet_bytes < 0 || g_wmap_packet_bytes > WMAP_PACKET_LIMIT || (u8*)frame->packet_cursor < packets ||
        (u8*)frame->packet_cursor > packets + WMAP_PACKET_BUFFER_BYTES)
    {
        fail_checkpoint("packet bounds");
    }

    lom_native_draw_labels_check("WMAP", ot);
    extend_mesh(index);
    DrawOTag((OT_TYPE*)ot);

    Psyz_VideoVSync(0);
    lom_native_frame_done();

    if (lom_native_gpu_quit_requested())
    {
        LOM_LONGJMP(s_checkpoint, 3);
    }
}

/**
 * @brief The world map's hook after each file read: stop it when a read or
 *        decompression fails, rather than let it use an old buffer.
 * @param call Which call it was.
 * @param resource The file's resource index.
 * @param result What the call returned.
 */
static void wmap_cd_after(LomNativeCdCall call, int32_t resource, int32_t result)
{
    if (call == LOM_NATIVE_CD_CALL_READ && lom_native_cd_get_last_error() != LOM_NATIVE_CD_ERROR_NONE)
    {
        fail_checkpoint("resource read");
    }

    if (call == LOM_NATIVE_CD_CALL_STREAM && (result < 0 || lom_native_cd_get_last_error() != LOM_NATIVE_CD_ERROR_NONE))
    {
        fprintf(stderr, "WMAP resource %04X error %d\n", (unsigned)resource, lom_native_cd_get_last_error());
        fail_checkpoint("resource decompression");
    }
}

static const LomNativeCdHooks wmap_cd_hooks = {NULL, wmap_cd_after};

MATRIX* lom_native_wmap_CompMatrix(MATRIX* left, MATRIX* right, MATRIX* output)
{
    MATRIX result = {0};
    SVECTOR column;
    VECTOR translated;
    SetRotMatrix(left);

    for (unsigned j = 0; j < 3; ++j)
    {
        column.vx = right->m[0][j];
        column.vy = right->m[1][j];
        column.vz = right->m[2][j];
        column.pad = 0;
        gte_ldv0(&column);
        Psyz_GteCommand(0x0486012);
        result.m[0][j] = Psyz_GteDataRead(9);
        result.m[1][j] = Psyz_GteDataRead(10);
        result.m[2][j] = Psyz_GteDataRead(11);
    }

    column.vx = right->t[0];
    column.vy = right->t[1];
    column.vz = right->t[2];
    gte_ldv0(&column);
    Psyz_GteCommand(0x0486012);
    gte_stlvnl(&translated);
    result.t[0] = (uint32_t)translated.vx + (uint32_t)left->t[0];
    result.t[1] = (uint32_t)translated.vy + (uint32_t)left->t[1];
    result.t[2] = (uint32_t)translated.vz + (uint32_t)left->t[2];
    *output = result;
    return output;
}

static int s_stop_status;

/**
 * @brief The game's guard abort while the world map runs: unwind out of it with
 *        the game's status.
 * @param status Why the game is stopping.
 */
static void wmap_abort(int status)
{
    s_stop_status = status;
    LOM_LONGJMP(s_checkpoint, 5);
}

/**
 * @brief Run the world map inside the port's guard: set its hooks, call @p
 *        enter, and turn however it ended into a status.
 * @param enter The steps that load and run it.
 * @param next_state Set to the state it left for.
 * @return LOM_NATIVE_GAME_RUNNING when it left normally, otherwise why it
 *         stopped.
 */
static LomNativeGameStatus run_wmap(int (*enter)(void), int32_t* next_state)
{
    int reason;

    if (s_active || !enter)
    {
        return LOM_NATIVE_GAME_ERROR;
    }

    s_active = 1;
    reason = LOM_SETJMP(s_checkpoint);

    if (!reason)
    {
        lom_native_psyz_set_quit_hook(wmap_quit);
        lom_native_cd_set_hooks(&wmap_cd_hooks);
        lom_native_game_set_guard(wmap_abort);
        *next_state = enter();
    }

    lom_native_game_set_guard(NULL);

    if (reason)
    {
        Psyz_VideoAbortVBlankDelivery();
    }

    lom_native_psyz_set_quit_hook(NULL);
    lom_native_cd_set_hooks(NULL);
    lom_native_widescreen_reset();
    s_active = 0;

    if (reason == 3)
    {
        return LOM_NATIVE_GAME_QUIT;
    }

    if (reason == 5)
    {
        return (LomNativeGameStatus)s_stop_status;
    }

    if (reason == 2)
    {
        return LOM_NATIVE_GAME_ERROR;
    }

    return LOM_NATIVE_GAME_RUNNING;
}

LomNativeGameStatus lom_native_wmap_run_entry(int (*enter)(void), int32_t* next_state)
{
    if (!next_state)
    {
        return LOM_NATIVE_GAME_ERROR;
    }

    return run_wmap(enter, next_state);
}
