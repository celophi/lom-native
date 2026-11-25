#include "common.h"
#include <inline_c.h>
#include <libgpu.h>
#include <libgte.h>
#include <rand.h>
#include "lom_native_input.h"
#include "lom_native_gpu_packets.h"

#include "../internal/field_actor.h"
#include "../internal/field_calls.h"
#include "../internal/field_effect_dispatch.h"
#include "../internal/field_effect_render_state.h"
#include "../internal/field_effect_types.h"
#include "common/gpu_packet.h"
#include "common/pad.h"
#include "main/controller_internal.h"
#include "main/display.h"
#include "main/main.h"
#include "sdk/gte_dmpsx_compat.h"

#define SCATTER_MIN_DISTANCE 64

#define EFFECT_LEVEL 160

#define EFFECT_BLUE(level) (((level) << 16) & 0xFFFFFF)

#define EFFECT_GREEN(level) (((level) << 8) & 0xFF00)

#define STRIP_COUNT 4
#define STRIP_SEGMENT_COUNT 8

#define EFFECT_SEGMENT_ANGLE (FIELD_ANGLE_TURN / 16)

#define DOME_ARCH_WIDTH 0x180

#define CURVE_STRIP_WIDTH 0x1400

#define CURVE_PHASE_PERIOD ((2 * CURVE_STRIP_WIDTH) >> 8)

#define SPIRAL_STRIP_WIDTH 20

#define SPIRAL_RADIUS_STEP 80
#define SPIRAL_RADIUS_LIMIT 320

#define GROUND_EFFECT_DOME_MIN_RADIUS 30
#define GROUND_EFFECT_DOME_MAX_RADIUS 96
#define GROUND_EFFECT_STRIPS_MIN_RADIUS 64
#define GROUND_EFFECT_STRIPS_MAX_RADIUS 128
#define GROUND_EFFECT_SPIRAL_MIN_RADIUS 16
#define GROUND_EFFECT_SPIRAL_MAX_RADIUS 64
#define GROUND_EFFECT_CURVES_MIN_RADIUS 30
#define GROUND_EFFECT_CURVES_MAX_RADIUS 200
#define GROUND_EFFECT_SCATTERED_DOMES_MIN_RADIUS 30
#define GROUND_EFFECT_SCATTERED_DOMES_MAX_RADIUS 64
#define GROUND_EFFECT_DRIFTING_DOME_MIN_RADIUS 0
#define GROUND_EFFECT_DRIFTING_DOME_MAX_RADIUS 100
#define GROUND_EFFECT_AIMED_DOME_MIN_RADIUS 30
#define GROUND_EFFECT_AIMED_DOME_MAX_RADIUS 64

#define EFFECT_SPIN_STEP 0x80

#define EFFECT_PHASE_STEP 4
#define EFFECT_PHASE_COUNT 80

#define OBJECT_STATE(actor) g_field_object_states[(actor)->object_index]

#define EFFECT_RADIUS(actor) (OBJECT_STATE(actor).movement.half.flags & FIELD_MOVEMENT_SCALE_MASK)

#define SET_EFFECT_RADIUS(actor, radius)                                                                                                                       \
    (OBJECT_STATE(actor).movement.word = (OBJECT_STATE(actor).movement.word & ~FIELD_MOVEMENT_SCALE_MASK) | ((radius) & FIELD_MOVEMENT_SCALE_MASK))

#define PROJECT_POINT(_poly, _vert, _point)                                                                                                                    \
    ((_poly)->x##_vert = (s16)FIELD_SCREEN_X((_point).vx), (_poly)->y##_vert = (s16)FIELD_SCREEN_Y((_point).vy, (_point).vz))

#define ADD_PRIM_BY_DEPTH(_ot, _cursor, _depth, _type)                                                                                                         \
    if ((_depth) < 0)                                                                                                                                          \
    {                                                                                                                                                          \
        lom_gpu_link(&(_ot)[0], (_cursor));                                                                                                                    \
        (_cursor) = (void*)((_type*)(_cursor) + 1);                                                                                                            \
    }                                                                                                                                                          \
    else if ((_depth) >= FIELD_WORLD_OT_SIZE)                                                                                                                  \
    {                                                                                                                                                          \
        lom_gpu_link(&(_ot)[FIELD_WORLD_OT_LAST], (_cursor));                                                                                                  \
        (_cursor) = (void*)((_type*)(_cursor) + 1);                                                                                                            \
    }                                                                                                                                                          \
    else                                                                                                                                                       \
    {                                                                                                                                                          \
        lom_gpu_link(&(_ot)[(_depth)], (_cursor));                                                                                                             \
        (_cursor) = (void*)((_type*)(_cursor) + 1);                                                                                                            \
    }

#define STRIP_POINT(_radius)                                                                                                                                   \
    input.vx = (_radius);                                                                                                                                      \
    input.vy = 0;                                                                                                                                              \
    input.vz = 0;                                                                                                                                              \
    gte_SetRotMatrix(matrix);                                                                                                                                  \
    gte_ldv0(&input);                                                                                                                                          \
    gte_rtv0();                                                                                                                                                \
    gte_stlvnl(&rotated);                                                                                                                                      \
    if (facing != 0)                                                                                                                                           \
    {                                                                                                                                                          \
        world.vx = position->vx + (rotated.vx << 8);                                                                                                           \
        world.vy = position->vy + (rotated.vy << 8);                                                                                                           \
        world.vz = position->vz + (rotated.vz << 8);                                                                                                           \
    }                                                                                                                                                          \
    else                                                                                                                                                       \
    {                                                                                                                                                          \
        world.vx = position->vx - (rotated.vx << 8);                                                                                                           \
        world.vy = position->vy + (rotated.vy << 8);                                                                                                           \
        world.vz = position->vz + (rotated.vz << 8);                                                                                                           \
    }

void field_get_ground_effect_radius_limits(s32 kind, s32* min_radius, s32* max_radius);

static u8* field_build_effect_dome(OT_TYPE* ordering_table, u8* packet, VECTOR* position, s32 radius);
static u8* field_build_effect_strips(OT_TYPE* ordering_table, u8* packet, VECTOR* position, s32 radius);
static u8* field_build_effect_curves(OT_TYPE* ordering_table, u8* packet, VECTOR* position, s32 extent, s32 forward);
static u8* field_build_effect_spiral(OT_TYPE* ordering_table, u8* packet, VECTOR* position, s32 tilt, s32 facing);
extern s32 g_field_effect_angle;

/**
 * @brief Start an object's ground effect.
 * @param actor Actor whose object gets the effect.
 * @param kind Effect kind (GroundEffectKind).
 * @note The radius starts at the kind's minimum (see field_get_ground_effect_radius_limits).
 */
void field_start_object_ground_effect(FieldActor* actor, u32 kind)
{
    VECTOR* delta = FIELD_DISTANCE_DELTA;
    VECTOR* squares = FIELD_DISTANCE_SQUARES;
    s32 point_index;
    s32 compare_index;
    s32 needs_retry;

    OBJECT_STATE(actor).effect_angle = 0;

    switch (kind)
    {
    case GROUND_EFFECT_STRIPS:
        SET_EFFECT_RADIUS(actor, GROUND_EFFECT_STRIPS_MIN_RADIUS);
        return;
    case GROUND_EFFECT_SPIRAL:
        SET_EFFECT_RADIUS(actor, GROUND_EFFECT_SPIRAL_MIN_RADIUS);
        return;
    case GROUND_EFFECT_DOME:
        SET_EFFECT_RADIUS(actor, GROUND_EFFECT_DOME_MIN_RADIUS);
        return;
    case GROUND_EFFECT_CURVES:
        SET_EFFECT_RADIUS(actor, GROUND_EFFECT_CURVES_MIN_RADIUS);
        return;
    case GROUND_EFFECT_SCATTERED_DOMES:
        SET_EFFECT_RADIUS(actor, GROUND_EFFECT_SCATTERED_DOMES_MIN_RADIUS);

        for (point_index = 0; point_index < FIELD_GROUND_EFFECT_POINT_COUNT; point_index++)
        {
            needs_retry = 1;

            while (needs_retry != 0)
            {
                OBJECT_STATE(actor).ground_effect_points[point_index].x = (rand() >> 7) - 128;
                OBJECT_STATE(actor).ground_effect_points[point_index].y = (rand() >> 7) - 128;
                OBJECT_STATE(actor).ground_effect_points[point_index].x =
                    OBJECT_STATE(actor).ground_effect_points[point_index].x - g_field_view_offset_x / 256 - actor->x / 256;
                OBJECT_STATE(actor).ground_effect_points[point_index].y =
                    OBJECT_STATE(actor).ground_effect_points[point_index].y - g_field_view_offset_z / 256 - actor->z / 256;
                for (compare_index = 0; compare_index < point_index; compare_index++)
                {
                    delta->vx = OBJECT_STATE(actor).ground_effect_points[compare_index].x - OBJECT_STATE(actor).ground_effect_points[point_index].x;
                    delta->vy = OBJECT_STATE(actor).ground_effect_points[compare_index].y - OBJECT_STATE(actor).ground_effect_points[point_index].y;
                    delta->vz = 0;
                    gte_ldlvl(delta);
                    gte_sqr0();
                    gte_stlvnl(squares);

                    if (SquareRoot0(squares->vx + squares->vy) < SCATTER_MIN_DISTANCE)
                    {
                        break;
                    }
                }

                if (compare_index == point_index)
                {
                    needs_retry = 0;
                }
            }
        }

        return;
    case GROUND_EFFECT_DRIFTING_DOME:
        SET_EFFECT_RADIUS(actor, GROUND_EFFECT_DRIFTING_DOME_MIN_RADIUS);
        OBJECT_STATE(actor).ground_effect_points[0].x = 0;
        OBJECT_STATE(actor).ground_effect_points[0].y = 0;
        return;
    case GROUND_EFFECT_AIMED_DOME:
        SET_EFFECT_RADIUS(actor, GROUND_EFFECT_AIMED_DOME_MIN_RADIUS);
        OBJECT_STATE(actor).ground_effect_points[0].x = 0;
        OBJECT_STATE(actor).ground_effect_points[0].y = 0;
        return;
    }
}

/**
 * @brief Look up the radius range of a ground effect kind.
 * @param kind Effect kind (GroundEffectKind); other values leave both outputs untouched.
 * @param min_radius Receives the radius at which the effect intensity is zero.
 * @param max_radius Receives the radius at which the effect stops growing.
 * @note Declared inline: field_draw_object_ground_effect expands it in place.
 */
inline void field_get_ground_effect_radius_limits(s32 kind, s32* min_radius, s32* max_radius)
{
    switch (kind)
    {
    case GROUND_EFFECT_DOME:
        *min_radius = GROUND_EFFECT_DOME_MIN_RADIUS;
        *max_radius = GROUND_EFFECT_DOME_MAX_RADIUS;
        break;
    case GROUND_EFFECT_STRIPS:
        *min_radius = GROUND_EFFECT_STRIPS_MIN_RADIUS;
        *max_radius = GROUND_EFFECT_STRIPS_MAX_RADIUS;
        break;
    case GROUND_EFFECT_SPIRAL:
        *min_radius = GROUND_EFFECT_SPIRAL_MIN_RADIUS;
        *max_radius = GROUND_EFFECT_SPIRAL_MAX_RADIUS;
        break;
    case GROUND_EFFECT_CURVES:
        *min_radius = GROUND_EFFECT_CURVES_MIN_RADIUS;
        *max_radius = GROUND_EFFECT_CURVES_MAX_RADIUS;
        break;
    case GROUND_EFFECT_SCATTERED_DOMES:
        *min_radius = GROUND_EFFECT_SCATTERED_DOMES_MIN_RADIUS;
        *max_radius = GROUND_EFFECT_SCATTERED_DOMES_MAX_RADIUS;
        break;
    case GROUND_EFFECT_DRIFTING_DOME:
        *min_radius = GROUND_EFFECT_DRIFTING_DOME_MIN_RADIUS;
        *max_radius = GROUND_EFFECT_DRIFTING_DOME_MAX_RADIUS;
        break;
    case GROUND_EFFECT_AIMED_DOME:
        *min_radius = GROUND_EFFECT_AIMED_DOME_MIN_RADIUS;
        *max_radius = GROUND_EFFECT_AIMED_DOME_MAX_RADIUS;
        break;
    }
}

/**
 * @brief Draw an object's ground effect and advance its radius and angle.
 * @param actor Actor supplying the position, facing and object index.
 * @param kind Effect kind (GroundEffectKind).
 * @note The aimed dome moves with the d-pad or left stick of the object's
 *       controller, but not past the screen edge.
 */
void field_draw_object_ground_effect(FieldActor* actor, u32 kind)
{
    VECTOR point;
    VECTOR direction;
    VECTOR target;
    DVECTOR screen;
    VECTOR* position;
    ControllerPortState* controller_ports = lom_input_ports();
    s32 intensity;
    s32 facing;
    s32 min_radius;
    s32 max_radius;
    s32 radius;
    s32 buttons;
    s32 raw_buttons;
    u16 held;
    s32 i;
    s32 draw;
    u8* packet;
    OT_TYPE* ordering_table;

    ordering_table = &g_field_render_half->ordering_table[FIELD_WORLD_OT_OFFSET];
    packet = g_field_render_half->primitive_cursor;
    field_get_ground_effect_radius_limits(kind, &min_radius, &max_radius);
    radius = EFFECT_RADIUS(actor);
    intensity = ((radius - min_radius) << 8) / (max_radius - min_radius);
    facing = actor->animation & FIELD_ANIMATION_FACING;
    OBJECT_STATE(actor).action_charge = intensity;
    position = (VECTOR*)&actor->x;

    if (OBJECT_STATE(actor).action_charge > FIELD_ACTION_CHARGE_FULL)
    {
        OBJECT_STATE(actor).action_charge = FIELD_ACTION_CHARGE_FULL;
    }

    draw = 1;

    if (actor->object_index == FIELD_PARTY_COMPANION)
    {
        if ((g_saved_game_ctx->characters[FIELD_PARTY_COMPANION].info.word & FIELD_CHARACTER_TYPE_MASK) == FIELD_CHARACTER_GOLEM)
        {
            draw = 0;
        }
    }

    g_field_effect_angle = OBJECT_STATE(actor).effect_angle;

    switch (kind)
    {
    case GROUND_EFFECT_DOME:
        if (draw != 0)
        {
            packet = field_build_effect_dome(ordering_table, packet, position, radius);
        }

        OBJECT_STATE(actor).effect_angle -= EFFECT_SPIN_STEP;

        if (EFFECT_RADIUS(actor) < max_radius)
        {
            SET_EFFECT_RADIUS(actor, EFFECT_RADIUS(actor) + 2);
        }

        break;
    case GROUND_EFFECT_STRIPS:
        if (draw != 0)
        {
            packet = field_build_effect_strips(ordering_table, packet, position, radius);
        }

        OBJECT_STATE(actor).effect_angle -= EFFECT_SPIN_STEP;

        if (EFFECT_RADIUS(actor) < max_radius)
        {
            SET_EFFECT_RADIUS(actor, EFFECT_RADIUS(actor) + 2);
        }

        break;
    case GROUND_EFFECT_SPIRAL:
        if (draw != 0)
        {
            packet = field_build_effect_spiral(ordering_table, packet, position, radius, facing);
        }

        OBJECT_STATE(actor).effect_angle += EFFECT_PHASE_STEP;

        if (OBJECT_STATE(actor).effect_angle >= EFFECT_PHASE_COUNT)
        {
            OBJECT_STATE(actor).effect_angle = 0;
        }

        if (EFFECT_RADIUS(actor) < max_radius)
        {
            SET_EFFECT_RADIUS(actor, EFFECT_RADIUS(actor) + 1);
        }

        break;
    case GROUND_EFFECT_CURVES:
        if (OBJECT_STATE(actor).effect_angle < 0)
        {
            OBJECT_STATE(actor).effect_angle = 0;
        }

        if (draw != 0)
        {
            packet = field_build_effect_curves(ordering_table, packet, position, radius, 0);
            packet = field_build_effect_curves(ordering_table, packet, position, radius, 1);
        }

        OBJECT_STATE(actor).effect_angle += EFFECT_PHASE_STEP;

        if (OBJECT_STATE(actor).effect_angle >= EFFECT_PHASE_COUNT)
        {
            OBJECT_STATE(actor).effect_angle = 0;
        }

        if (EFFECT_RADIUS(actor) < max_radius)
        {
            SET_EFFECT_RADIUS(actor, EFFECT_RADIUS(actor) + 2);
        }

        break;
    case GROUND_EFFECT_SCATTERED_DOMES:
        for (i = 0; i < FIELD_GROUND_EFFECT_POINT_COUNT; i++)
        {
            point.vx = actor->x + (OBJECT_STATE(actor).ground_effect_points[i].x << 8);
            point.vy = actor->y;
            point.vz = actor->z + (OBJECT_STATE(actor).ground_effect_points[i].y << 8);

            if (draw != 0)
            {
                packet = field_build_effect_dome(ordering_table, packet, &point, radius);
            }
        }

        if (EFFECT_RADIUS(actor) < max_radius)
        {
            SET_EFFECT_RADIUS(actor, EFFECT_RADIUS(actor) + 2);
        }

        OBJECT_STATE(actor).effect_angle -= EFFECT_SPIN_STEP;
        break;
    case GROUND_EFFECT_DRIFTING_DOME:
        point.vx = actor->x + (OBJECT_STATE(actor).ground_effect_points[0].x << 8);
        point.vy = actor->y;
        point.vz = actor->z + (OBJECT_STATE(actor).ground_effect_points[0].y << 8);

        if (draw != 0)
        {
            packet = field_build_effect_dome(ordering_table, packet, &point, radius);
        }

        OBJECT_STATE(actor).effect_angle -= EFFECT_SPIN_STEP;

        if (EFFECT_RADIUS(actor) < max_radius)
        {
            SET_EFFECT_RADIUS(actor, EFFECT_RADIUS(actor) + 2);

            if (actor->animation & FIELD_ANIMATION_FACING)
            {
                OBJECT_STATE(actor).ground_effect_points[0].x += 2;
            }
            else
            {
                OBJECT_STATE(actor).ground_effect_points[0].x -= 2;
            }
        }

        break;
    case GROUND_EFFECT_AIMED_DOME:
        point.vx = actor->x + (OBJECT_STATE(actor).ground_effect_points[0].x << 8);
        point.vy = actor->y;
        point.vz = actor->z + (OBJECT_STATE(actor).ground_effect_points[0].y << 8);

        if (draw != 0)
        {
            packet = field_build_effect_dome(ordering_table, packet, &point, radius);
        }

        OBJECT_STATE(actor).effect_angle -= EFFECT_SPIN_STEP;

        if (EFFECT_RADIUS(actor) < max_radius)
        {
            SET_EFFECT_RADIUS(actor, EFFECT_RADIUS(actor) + 1);
        }

        if (actor->object_index < FIELD_PLAYER_COUNT)
        {
            if (controller_ports[actor->object_index].published_sample.device_type >= CONTROLLER_DEVICE_CONFIGURING)
            {
                raw_buttons = 0;
            }
            else
            {
                held = controller_ports[actor->object_index].published_sample.held_buttons;
                raw_buttons = PAD_SWAP_BYTES(held);
            }
#if defined(VERSION_JP)
            buttons = raw_buttons;
#else
            buttons = PAD_REMAP_FACE_BITS_16(raw_buttons);
#endif
            point.vz = 0;
            point.vy = 0;
            point.vx = 0;

            if (buttons & PAD_BTN_RIGHT)
            {
                point.vx = ONE;
            }

            if (buttons & PAD_BTN_LEFT)
            {
                point.vx -= ONE;
            }

            if (buttons & PAD_BTN_DOWN)
            {
                point.vy = -ONE;
            }

            if (buttons & PAD_BTN_UP)
            {
                point.vy += ONE;
            }

            if (controller_ports[actor->object_index].published_sample.device_type != CONTROLLER_DEVICE_DIGITAL)
            {
                point.vx += controller_ports[actor->object_index].published_sample.left_stick_x * 16;
                point.vy -= controller_ports[actor->object_index].published_sample.left_stick_y * 16;
            }

            if ((point.vx | point.vy) != 0)
            {
                VectorNormal(&point, &direction);
                target.vx = actor->x + ((OBJECT_STATE(actor).ground_effect_points[0].x + (direction.vx >> 10)) << 8);
                target.vy = actor->y;
                target.vz = actor->z + ((OBJECT_STATE(actor).ground_effect_points[0].y + (direction.vy >> 10)) << 8);
                screen.vx = FIELD_SCREEN_X(target.vx);
                screen.vy = FIELD_SCREEN_Y(target.vy, target.vz);

                if ((screen.vx > 0 || direction.vx > 0) && (screen.vy > 0 || direction.vy < 0) && (screen.vx < SCREEN_WIDTH || direction.vx < 0) &&
                    (screen.vy < VRAM_DRAW_HEIGHT || direction.vy > 0))
                {
                    OBJECT_STATE(actor).ground_effect_points[0].x += direction.vx >> 10;
                    OBJECT_STATE(actor).ground_effect_points[0].y += direction.vy >> 10;
                }
            }
        }

        break;
    }

    g_field_render_half->primitive_cursor = packet;
}

/**
 * @brief Append a rotating dome of arched quads around a position.
 * @param ordering_table Ordering table with FIELD_WORLD_OT_SIZE depth buckets.
 * @param packet Next free primitive-buffer byte.
 * @param position World-space center in fixed-point coordinates.
 * @param radius Dome radius.
 * @return First free byte after the appended primitives.
 * @note The dome is rotated by the effect angle in g_field_effect_angle.
 */
static u8* field_build_effect_dome(OT_TYPE* ordering_table, u8* packet, VECTOR* position, s32 radius)
{
    VECTOR v[6];
    SVECTOR rot;
    MATRIX m0;
    MATRIX m1;
    s32 i;
    s32 x;
    s32 y;
    s32 initial_x;
    s32 initial_y;
    u32 center;
    POLY_G4* cursor;

    cursor = (POLY_G4*)packet;

    rot.vx = 0;
    rot.vz = 0;
    rot.vy = g_field_effect_angle;
    RotMatrix_gte(&rot, &m0);

    initial_x = (rcos(0) >> 4) * radius;
    initial_y = (rsin(0) >> 4) * radius;

    rot.vx = 0;
    rot.vz = 0;
    rot.vy = g_field_effect_angle + DOME_ARCH_WIDTH;
    RotMatrix_gte(&rot, &m1);

    v[0].vx = initial_x;
    v[0].vy = initial_y;
    v[0].vz = 0;
    ApplyMatrixLV(&m0, &v[0], &v[1]);
    v[0].vx = initial_x;
    v[0].vy = initial_y;
    v[0].vz = 0;
    ApplyMatrixLV(&m1, &v[0], &v[4]);

    v[0].vx = position->vx + v[1].vx;
    v[0].vy = position->vy + v[1].vy;
    v[0].vz = position->vz + v[1].vz;
    PROJECT_POINT(cursor, 0, v[0]);

    v[0].vx = position->vx - v[1].vx;
    v[0].vy = position->vy + v[1].vy;
    v[0].vz = position->vz - v[1].vz;
    PROJECT_POINT(&cursor[1], 0, v[0]);

    v[0].vx = position->vx - v[1].vz;
    v[0].vy = position->vy + v[1].vy;
    v[0].vz = position->vz + v[1].vx;
    PROJECT_POINT(&cursor[2], 0, v[0]);

    v[0].vx = position->vx + v[1].vz;
    v[0].vy = position->vy + v[1].vy;
    v[0].vz = position->vz - v[1].vx;
    PROJECT_POINT(&cursor[3], 0, v[0]);

    v[0].vx = position->vx;
    v[0].vy = position->vy;
    v[0].vz = position->vz;
    PROJECT_POINT(cursor, 1, v[0]);
    center = *(s32*)&cursor->x1;
    SET_XY3_PACKED(cursor, center);
    SET_XY1_PACKED(&cursor[3], center);
    SET_XY3_PACKED(&cursor[3], center);
    SET_XY1_PACKED(&cursor[2], center);
    SET_XY3_PACKED(&cursor[2], center);
    SET_XY1_PACKED(&cursor[1], center);
    SET_XY3_PACKED(&cursor[1], center);

    v[0].vx = position->vx + v[4].vx;
    v[0].vy = position->vy + v[4].vy;
    v[0].vz = position->vz + v[4].vz;
    PROJECT_POINT(cursor, 2, v[0]);

    v[0].vx = position->vx - v[4].vx;
    v[0].vy = position->vy + v[4].vy;
    v[0].vz = position->vz - v[4].vz;
    PROJECT_POINT(&cursor[1], 2, v[0]);

    v[0].vx = position->vx - v[4].vz;
    v[0].vy = position->vy + v[4].vy;
    v[0].vz = position->vz + v[4].vx;
    PROJECT_POINT(&cursor[2], 2, v[0]);

    v[0].vx = position->vx + v[4].vz;
    v[0].vy = position->vy + v[4].vy;
    v[0].vz = position->vz - v[4].vx;
    PROJECT_POINT(&cursor[3], 2, v[0]);

    SET_BGR0_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL - (v[1].vz >> 8)));
    SET_BGR0_PACKED(&cursor[1], EFFECT_GREEN((v[1].vz >> 8) + EFFECT_LEVEL));
    SET_BGR0_PACKED(&cursor[2], EFFECT_GREEN(EFFECT_LEVEL - (v[1].vx >> 8)));
    SET_BGR0_PACKED(&cursor[3], EFFECT_GREEN((v[1].vx >> 8) + EFFECT_LEVEL));

    SET_POLY_G4_BGR1_PACKED(&cursor[3], EFFECT_GREEN(EFFECT_LEVEL));
    SET_POLY_G4_BGR1_PACKED(&cursor[2], EFFECT_GREEN(EFFECT_LEVEL));
    SET_POLY_G4_BGR1_PACKED(&cursor[1], EFFECT_GREEN(EFFECT_LEVEL));
    SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_GREEN(EFFECT_LEVEL));
    SET_POLY_G4_BGR3_PACKED(&cursor[1], 0);
    SET_POLY_G4_BGR2_PACKED(&cursor[1], 0);
    SET_POLY_G4_BGR3_PACKED(&cursor[2], 0);
    SET_POLY_G4_BGR2_PACKED(&cursor[2], 0);
    SET_POLY_G4_BGR3_PACKED(&cursor[3], 0);
    SET_POLY_G4_BGR2_PACKED(&cursor[3], 0);
    SET_POLY_G4_BGR3_PACKED(cursor, 0);
    SET_POLY_G4_BGR2_PACKED(cursor, 0);

    lom_gpu_prepare_poly_g4(cursor);
    lom_gpu_prepare_poly_g4(&cursor[1]);
    lom_gpu_prepare_poly_g4(&cursor[2]);
    lom_gpu_prepare_poly_g4(&cursor[3]);
    lom_gpu_set_semi_trans(cursor, 1);
    lom_gpu_set_semi_trans(&cursor[1], 1);
    lom_gpu_set_semi_trans(&cursor[2], 1);
    lom_gpu_set_semi_trans(&cursor[3], 1);

    ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
    ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
    ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
    ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);

    lom_gpu_init_tpage_packet((DR_TPAGE*)cursor, 0, 0, BLEND_ADD_TPAGE);
    ADD_PRIM_BY_DEPTH(ordering_table, cursor, (position->vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, DR_TPAGE);

    for (i = 1; i <= STRIP_SEGMENT_COUNT; i++)
    {
        x = (rcos(i * EFFECT_SEGMENT_ANGLE) >> 4) * radius;
        y = -(rsin(i * EFFECT_SEGMENT_ANGLE) >> 4) * radius;
        v[0].vx = x;
        v[0].vy = y;
        v[0].vz = 0;
        ApplyMatrixLV(&m0, &v[0], &v[2]);
        v[0].vx = x;
        v[0].vy = y;
        v[0].vz = 0;
        ApplyMatrixLV(&m1, &v[0], &v[5]);

        v[0].vx = position->vx + v[1].vx;
        v[0].vy = position->vy + v[1].vy;
        v[0].vz = position->vz + v[1].vz;
        PROJECT_POINT(cursor, 0, v[0]);
        v[0].vx = position->vx + v[2].vx;
        v[0].vy = position->vy + v[2].vy;
        v[0].vz = position->vz + v[2].vz;
        PROJECT_POINT(cursor, 1, v[0]);
        v[0].vx = position->vx + v[4].vx;
        v[0].vy = position->vy + v[4].vy;
        v[0].vz = position->vz + v[4].vz;
        PROJECT_POINT(cursor, 2, v[0]);
        v[0].vx = position->vx + v[5].vx;
        v[0].vy = position->vy + v[5].vy;
        v[0].vz = position->vz + v[5].vz;
        PROJECT_POINT(cursor, 3, v[0]);

        SET_BGR0_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL - (v[1].vz >> 8)));
        SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL - (v[2].vz >> 8)));
        SET_POLY_G4_BGR2_PACKED(cursor, 0);
        SET_POLY_G4_BGR3_PACKED(cursor, 0);
        lom_gpu_prepare_poly_g4(cursor);
        lom_gpu_set_semi_trans(cursor, 1);
        ADD_PRIM_BY_DEPTH(ordering_table, cursor, (position->vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);

        lom_gpu_init_tpage_packet((DR_TPAGE*)cursor, 0, 0, BLEND_ADD_TPAGE);
        ADD_PRIM_BY_DEPTH(ordering_table, cursor, (position->vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, DR_TPAGE);

        v[0].vx = position->vx - v[1].vz;
        v[0].vy = position->vy + v[1].vy;
        v[0].vz = position->vz + v[1].vx;
        PROJECT_POINT(cursor, 0, v[0]);
        v[0].vx = position->vx - v[2].vz;
        v[0].vy = position->vy + v[2].vy;
        v[0].vz = position->vz + v[2].vx;
        PROJECT_POINT(cursor, 1, v[0]);
        v[0].vx = position->vx - v[4].vz;
        v[0].vy = position->vy + v[4].vy;
        v[0].vz = position->vz + v[4].vx;
        PROJECT_POINT(cursor, 2, v[0]);
        v[0].vx = position->vx - v[5].vz;
        v[0].vy = position->vy + v[5].vy;
        v[0].vz = position->vz + v[5].vx;
        PROJECT_POINT(cursor, 3, v[0]);

        SET_BGR0_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL - (v[1].vz >> 8)));
        SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL - (v[2].vz >> 8)));
        SET_POLY_G4_BGR2_PACKED(cursor, 0);
        SET_POLY_G4_BGR3_PACKED(cursor, 0);
        lom_gpu_prepare_poly_g4(cursor);
        lom_gpu_set_semi_trans(cursor, 1);
        ADD_PRIM_BY_DEPTH(ordering_table, cursor, (position->vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);

        lom_gpu_init_tpage_packet((DR_TPAGE*)cursor, 0, 0, BLEND_ADD_TPAGE);
        ADD_PRIM_BY_DEPTH(ordering_table, cursor, (position->vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, DR_TPAGE);

        v[1].vx = v[2].vx;
        v[1].vy = v[2].vy;
        v[1].vz = v[2].vz;
        v[4].vx = v[5].vx;
        v[4].vy = v[5].vy;
        v[4].vz = v[5].vz;
    }

    return (u8*)cursor;
}

/**
 * @brief Append four rotating strips of arched quads.
 * @param ordering_table Ordering table with FIELD_WORLD_OT_SIZE depth buckets.
 * @param packet Next free primitive-buffer byte.
 * @param position World-space center in fixed-point coordinates.
 * @param radius Radius used to construct the strips.
 * @return First free byte after the appended primitives.
 */
static u8* field_build_effect_strips(OT_TYPE* ordering_table, u8* packet, VECTOR* position, s32 radius)
{
    VECTOR v[6];
    SVECTOR rot;
    MATRIX m0;
    MATRIX unused_matrix;
    VECTOR p0;
    VECTOR p1;
    s32 distance;
    s32 i;
    s32 j;
    s32 x;
    s32 y;
    s32 inner_x;
    s32 inner_y;
    s32 angle;
    POLY_G4* cursor;

    cursor = (POLY_G4*)packet;

    for (i = 0; i < STRIP_COUNT; i++)
    {
        distance = (radius >> 1) + GROUND_EFFECT_STRIPS_INNER_RADIUS;
        p0.vx = position->vx;
        p0.vy = position->vy;
        p0.vz = position->vz;
        p1.vx = position->vx;
        p1.vy = position->vy;
        p1.vz = position->vz;
        angle = i * FIELD_ANGLE_QUARTER_TURN;
        x = (rcos(angle - g_field_effect_angle) >> 4) * distance;
        y = (rsin(angle - g_field_effect_angle) >> 4) * distance;
        p0.vx += x;
        p0.vz += y;
        x = (rcos(angle - g_field_effect_angle - EFFECT_SEGMENT_ANGLE) >> 4) * distance;
        y = (rsin(angle - g_field_effect_angle - EFFECT_SEGMENT_ANGLE) >> 4) * distance;
        p1.vx += x;
        p1.vz += y;
        rot.vx = 0;
        rot.vz = 0;
        rot.vy = g_field_effect_angle + angle;
        RotMatrix_gte(&rot, &m0);
        x = ((rcos(0) >> 4) * radius) >> 1;
        y = ((rsin(0) >> 4) * radius) >> 1;
        v[0].vx = x;
        v[0].vy = y;
        v[0].vz = 0;
        ApplyMatrixLV(&m0, &v[0], &v[1]);
        v[0].vx = -radius * 0x80;
        v[0].vy = 0;
        v[0].vz = 0;
        ApplyMatrixLV(&m0, &v[0], &v[2]);
        v[0].vx = p0.vx + v[1].vx;
        v[0].vy = p0.vy + v[1].vy;
        v[0].vz = p0.vz + v[1].vz;
        PROJECT_POINT(cursor, 0, v[0]);
        v[0].vx = p0.vx + v[2].vx;
        v[0].vy = p0.vy + v[2].vy;
        v[0].vz = p0.vz + v[2].vz;
        PROJECT_POINT(cursor, 1, v[0]);
        v[0].vx = p1.vx + v[1].vx;
        v[0].vy = p1.vy + v[1].vy;
        v[0].vz = p1.vz + v[1].vz;
        PROJECT_POINT(cursor, 2, v[0]);
        v[0].vx = p1.vx + v[2].vx;
        v[0].vy = p1.vy + v[2].vy;
        v[0].vz = p1.vz + v[2].vz;
        PROJECT_POINT(cursor, 3, v[0]);
        SET_BGR0_PACKED(cursor, EFFECT_GREEN(EFFECT_LEVEL - (v[1].vz >> 8)));
        SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_GREEN(EFFECT_LEVEL - (v[2].vz >> 8)));
        SET_POLY_G4_BGR2_PACKED(cursor, 0);
        SET_POLY_G4_BGR3_PACKED(cursor, 0);
        lom_gpu_prepare_poly_g4(cursor);
        lom_gpu_set_semi_trans(cursor, 1);
        ADD_PRIM_BY_DEPTH(ordering_table, cursor, (p0.vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
        lom_gpu_init_tpage_packet(cursor, 0, 0, BLEND_ADD_TPAGE);
        ADD_PRIM_BY_DEPTH(ordering_table, cursor, (p0.vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, DR_TPAGE);

        for (j = 1; j <= STRIP_SEGMENT_COUNT; j++)
        {
            inner_x = ((rcos(j * EFFECT_SEGMENT_ANGLE) >> 4) * radius) >> 1;
            inner_y = (-(rsin(j * EFFECT_SEGMENT_ANGLE) >> 4) * radius) >> 1;
            v[0].vx = inner_x;
            v[0].vy = inner_y;
            v[0].vz = 0;
            ApplyMatrixLV(&m0, &v[0], &v[2]);
            v[0].vx = p0.vx + v[1].vx;
            v[0].vy = p0.vy + v[1].vy;
            v[0].vz = p0.vz + v[1].vz;
            PROJECT_POINT(cursor, 0, v[0]);
            v[0].vx = p0.vx + v[2].vx;
            v[0].vy = p0.vy + v[2].vy;
            v[0].vz = p0.vz + v[2].vz;
            PROJECT_POINT(cursor, 1, v[0]);
            v[0].vx = p1.vx + v[1].vx;
            v[0].vy = p1.vy + v[1].vy;
            v[0].vz = p1.vz + v[1].vz;
            PROJECT_POINT(cursor, 2, v[0]);
            v[0].vx = p1.vx + v[2].vx;
            v[0].vy = p1.vy + v[2].vy;
            v[0].vz = p1.vz + v[2].vz;
            PROJECT_POINT(cursor, 3, v[0]);
            SET_BGR0_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL - (v[1].vz >> 8)));
            SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL - (v[2].vz >> 8)));
            SET_POLY_G4_BGR2_PACKED(cursor, 0);
            SET_POLY_G4_BGR3_PACKED(cursor, 0);
            lom_gpu_prepare_poly_g4(cursor);
            lom_gpu_set_semi_trans(cursor, 1);
            ADD_PRIM_BY_DEPTH(ordering_table, cursor, (p0.vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
            lom_gpu_init_tpage_packet(cursor, 0, 0, BLEND_ADD_TPAGE);
            ADD_PRIM_BY_DEPTH(ordering_table, cursor, (p0.vz + v[1].vz) >> FIELD_WORLD_OT_DEPTH_SHIFT, DR_TPAGE);
            v[1].vx = v[2].vx;
            v[1].vy = v[2].vy;
            v[1].vz = v[2].vz;
        }
    }

    return (u8*)cursor;
}

/**
 * @brief Draw animated curved quad strips running out along X from a position.
 * @param ordering_table Ordering table with FIELD_WORLD_OT_SIZE depth buckets.
 * @param packet Destination for the generated GPU packets.
 * @param position World position in signed fixed-point coordinates.
 * @param extent Maximum horizontal extent, tested after each completed strip.
 * @param forward Nonzero runs toward positive X; zero toward negative X.
 * @return First byte after the emitted primitives and draw-page command.
 * @note Emits at least one strip and at most four, with nine quads per strip.
 */
static u8* field_build_effect_curves(OT_TYPE* ordering_table, u8* packet, VECTOR* position, s32 extent, s32 forward)
{
    VECTOR rotated;
    VECTOR world;
    VECTOR unused;
    SVECTOR input;
    MATRIX unused_matrices[2];
    MATRIX rotation;
    MATRIX unused_matrices_after[2];
    s32 step;
    s32 strip_index;
    s32 angle;
    s32 offset;
    s32 first_xy;
    s32 second_xy;
    POLY_G4* cursor;

    cursor = (POLY_G4*)packet;
    strip_index = 0;
    offset = (g_field_effect_angle % CURVE_PHASE_PERIOD) << 8;

    do
    {
        if (forward != 0)
        {
            world.vx = position->vx + offset;
            world.vy = position->vy;
            world.vz = position->vz + GROUND_EFFECT_CURVE_RADIUS;
        }
        else
        {
            world.vx = position->vx - offset;
            world.vy = position->vy;
            world.vz = position->vz + GROUND_EFFECT_CURVE_RADIUS;
        }

        PROJECT_POINT(cursor, 0, world);
        first_xy = *(s32*)&cursor->x0;

        if (forward != 0)
        {
            world.vx = position->vx + offset + CURVE_STRIP_WIDTH;
            world.vy = position->vy;
            world.vz = position->vz + GROUND_EFFECT_CURVE_RADIUS;
        }
        else
        {
            world.vx = position->vx - offset - CURVE_STRIP_WIDTH;
            world.vy = position->vy;
            world.vz = position->vz + GROUND_EFFECT_CURVE_RADIUS;
        }

        PROJECT_POINT(cursor, 1, world);
        second_xy = *(s32*)&cursor->x1;

        for (step = 1, angle = EFFECT_SEGMENT_ANGLE; step <= STRIP_SEGMENT_COUNT; step++, angle += EFFECT_SEGMENT_ANGLE)
        {
            if (forward != 0)
            {
                world.vx = position->vx + offset + angle;
                world.vy = position->vy - rsin(angle) * 2;
                world.vz = position->vz + rcos(angle) * 2;
            }
            else
            {
                world.vx = position->vx - offset - angle;
                world.vy = position->vy - rsin(angle) * 2;
                world.vz = position->vz + rcos(angle) * 2;
            }

            PROJECT_POINT(cursor, 2, world);
            SET_XY0_PACKED(&cursor[1], *(s32*)&cursor->x2);

            if (forward != 0)
            {
                world.vx = position->vx + offset + angle + CURVE_STRIP_WIDTH;
                world.vy = position->vy - rsin(angle) * 2;
                world.vz = position->vz + rcos(angle) * 2;
            }
            else
            {
                world.vx = position->vx - offset - angle - CURVE_STRIP_WIDTH;
                world.vy = position->vy - rsin(angle) * 2;
                world.vz = position->vz + rcos(angle) * 2;
            }

            PROJECT_POINT(cursor, 3, world);
            SET_BGR0_PACKED(cursor, 0);
            SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL));
            SET_POLY_G4_BGR2_PACKED(cursor, 0);
            SET_POLY_G4_BGR3_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL));
            SET_XY1_PACKED(&cursor[1], *(s32*)&cursor->x3);
            lom_gpu_prepare_poly_g4(cursor);
            lom_gpu_set_semi_trans(cursor, 1);
            ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
        }

        SET_XY2_PACKED(cursor, first_xy);
        SET_XY3_PACKED(cursor, second_xy);
        SET_BGR0_PACKED(cursor, 0);
        SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_GREEN(EFFECT_LEVEL));
        SET_POLY_G4_BGR2_PACKED(cursor, 0);
        SET_POLY_G4_BGR3_PACKED(cursor, EFFECT_GREEN(EFFECT_LEVEL));
        lom_gpu_prepare_poly_g4(cursor);
        lom_gpu_set_semi_trans(cursor, 1);
        ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
        offset += 2 * CURVE_STRIP_WIDTH;
    } while (offset < (extent << 8) && ++strip_index < STRIP_COUNT);

    lom_gpu_init_tpage_packet((DR_TPAGE*)cursor, 0, 0, BLEND_ADD_TPAGE);
    ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, DR_TPAGE);
    return (u8*)cursor;
}

/**
 * @brief Draw four strips of quads wound around a tilted axis.
 * @param ordering_table Ordering table with FIELD_WORLD_OT_SIZE depth buckets.
 * @param packet First free primitive packet.
 * @param position Fixed-point world-space origin of the effect.
 * @param tilt Effect radius; the axis turns by GROUND_EFFECT_SPIRAL_ANGLE(tilt).
 * @param facing Selects addition or subtraction of the rotated X offset.
 * @return First free packet following all quads and the draw-page command.
 * @note Each strip turns a half circle around X in eight steps while its
 *       radius grows; the phase in g_field_effect_angle is the radius of the first strip.
 */
static u8* field_build_effect_spiral(OT_TYPE* ordering_table, u8* packet, VECTOR* position, s32 tilt, s32 facing)
{
    VECTOR rotated;
    VECTOR world;
    VECTOR unused;
    SVECTOR input;
    MATRIX unused_matrices[2];
    MATRIX rotation;
    MATRIX unused_matrices_after[2];
    s32 strip_index;
    s32 angle;
    s32 first_xy;
    s32 second_xy;
    MATRIX* matrix;
    s32 segment_index;
    s32 radius;
    s32 radius_sum;
    POLY_G4* cursor;

    cursor = (POLY_G4*)packet;
    angle = GROUND_EFFECT_SPIRAL_ANGLE(tilt);
    strip_index = 0;
    matrix = &rotation;
    radius = g_field_effect_angle;

    for (; strip_index < STRIP_COUNT; strip_index++)
    {
        *(s32*)&rotation.m[2][2] = ONE;
        *(s32*)&rotation.m[1][1] = ONE;
        *(s32*)&rotation.m[0][0] = ONE;
        rotation.t[2] = 0;
        rotation.t[1] = 0;
        rotation.t[0] = 0;
        *(s32*)&rotation.m[2][0] = 0;
        *(s32*)&rotation.m[0][2] = 0;
        RotMatrixY(angle, matrix);
        STRIP_POINT(radius);
        PROJECT_POINT(cursor, 0, world);
        first_xy = *(s32*)&cursor->x0;
        STRIP_POINT(radius + SPIRAL_STRIP_WIDTH);
        PROJECT_POINT(cursor, 1, world);
        second_xy = *(s32*)&cursor->x1;

        for (segment_index = 1, radius_sum = radius; segment_index <= STRIP_SEGMENT_COUNT; segment_index++, radius_sum += radius)
        {
            RotMatrixX(-EFFECT_SEGMENT_ANGLE, matrix);
            STRIP_POINT(radius + (radius_sum >> 5));
            PROJECT_POINT(cursor, 2, world);
            SET_XY0_PACKED(&cursor[1], *(s32*)&cursor->x2);
            STRIP_POINT(radius + (radius_sum >> 5) + SPIRAL_STRIP_WIDTH);
            PROJECT_POINT(cursor, 3, world);
            SET_BGR0_PACKED(cursor, 0);
            SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL));
            SET_POLY_G4_BGR2_PACKED(cursor, 0);
            SET_POLY_G4_BGR3_PACKED(cursor, EFFECT_BLUE(EFFECT_LEVEL));
            SET_XY1_PACKED(&cursor[1], *(s32*)&cursor->x3);
            lom_gpu_prepare_poly_g4(cursor);
            lom_gpu_set_semi_trans(cursor, 1);
            ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
        }

        SET_XY2_PACKED(cursor, first_xy);
        SET_XY3_PACKED(cursor, second_xy);
        SET_BGR0_PACKED(cursor, 0);
        SET_POLY_G4_BGR1_PACKED(cursor, EFFECT_GREEN(EFFECT_LEVEL));
        SET_POLY_G4_BGR2_PACKED(cursor, 0);
        SET_POLY_G4_BGR3_PACKED(cursor, EFFECT_GREEN(EFFECT_LEVEL));
        lom_gpu_prepare_poly_g4(cursor);
        lom_gpu_set_semi_trans(cursor, 1);
        ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, POLY_G4);
        radius += SPIRAL_RADIUS_STEP;

        if (radius >= SPIRAL_RADIUS_LIMIT)
        {
            radius -= SPIRAL_RADIUS_LIMIT;
        }
    }

    lom_gpu_init_tpage_packet((DR_TPAGE*)cursor, 0, 0, BLEND_ADD_TPAGE);
    ADD_PRIM_BY_DEPTH(ordering_table, cursor, position->vz >> FIELD_WORLD_OT_DEPTH_SHIFT, DR_TPAGE);
    return (u8*)cursor;
}
