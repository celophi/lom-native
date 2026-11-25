#ifndef WMAP_STEP_SEQUENCE_H
#define WMAP_STEP_SEQUENCE_H

#include "wmap_sequence_runtime.h"

#define WMAP_STEP_RUNNER(name, table, count, step, timer)                                                                                                      \
    s32 name(s32 reset)                                                                                                                                        \
    {                                                                                                                                                          \
        s32 result;                                                                                                                                            \
                                                                                                                                                               \
        if (reset != 0)                                                                                                                                        \
        {                                                                                                                                                      \
            step = 1;                                                                                                                                          \
            timer = 1;                                                                                                                                         \
            return 1;                                                                                                                                          \
        }                                                                                                                                                      \
                                                                                                                                                               \
        if (step < count)                                                                                                                                      \
        {                                                                                                                                                      \
            table[step]();                                                                                                                                     \
            result = 1;                                                                                                                                        \
        }                                                                                                                                                      \
        else                                                                                                                                                   \
        {                                                                                                                                                      \
            result = 0;                                                                                                                                        \
        }                                                                                                                                                      \
        return result;                                                                                                                                         \
    }

#define WMAP_STEP_RUNNER_RESET_AND_RUN(name, table, count, step, timer)                                                                                        \
    s32 name(s32 reset)                                                                                                                                        \
    {                                                                                                                                                          \
        s32 result;                                                                                                                                            \
                                                                                                                                                               \
        if (reset != 0)                                                                                                                                        \
        {                                                                                                                                                      \
            step = 1;                                                                                                                                          \
            timer = 1;                                                                                                                                         \
        }                                                                                                                                                      \
                                                                                                                                                               \
        if (step < count)                                                                                                                                      \
        {                                                                                                                                                      \
            table[step]();                                                                                                                                     \
            result = 1;                                                                                                                                        \
        }                                                                                                                                                      \
        else                                                                                                                                                   \
        {                                                                                                                                                      \
            result = 0;                                                                                                                                        \
        }                                                                                                                                                      \
        return result;                                                                                                                                         \
    }

#define WMAP_STEP_RESET(name, step, timer)                                                                                                                     \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        step = 1;                                                                                                                                              \
        timer = 1;                                                                                                                                             \
    }

#define WMAP_STEP_WAIT(name, step, timer)                                                                                                                      \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        if (--timer == 0)                                                                                                                                      \
        {                                                                                                                                                      \
            step++;                                                                                                                                            \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_UPDATE_AND_WAIT(name, step, timer, update_call)                                                                                              \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        update_call;                                                                                                                                           \
        if (--(timer) == 0)                                                                                                                                    \
        {                                                                                                                                                      \
            (step)++;                                                                                                                                          \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_UPDATE_AND_RAMP(name, step, timer, value, amount, update_call)                                                                               \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        s32 remaining;                                                                                                                                         \
                                                                                                                                                               \
        update_call;                                                                                                                                           \
        (value) += (amount);                                                                                                                                   \
        remaining = (timer) - 1;                                                                                                                               \
        (timer) = remaining;                                                                                                                                   \
        if (remaining == 0)                                                                                                                                    \
        {                                                                                                                                                      \
            (step)++;                                                                                                                                          \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_UPDATE_TWO_AND_WAIT(name, step, timer, first_call, second_call)                                                                              \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        first_call;                                                                                                                                            \
        second_call;                                                                                                                                           \
        if (--(timer) == 0)                                                                                                                                    \
        {                                                                                                                                                      \
            (step)++;                                                                                                                                          \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_START_ACTOR(name, step, timer, slot, animation, sequence_id, initial_shade, shade_target, shade_delta, frames, next)                         \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        WmapSpriteActor* actor = &g_wmap_sprite_actors[(slot)];                                                                                                \
                                                                                                                                                               \
        g_wmap_actor_animations[(slot)].data = (animation);                                                                                                    \
        actor->scale_index = WMAP_SPRITE_SCALE_FULL;                                                                                                           \
        if ((sequence_id) != 0)                                                                                                                                \
        {                                                                                                                                                      \
            actor->sequence = (sequence_id);                                                                                                                   \
        }                                                                                                                                                      \
        actor->previous_sequence = -1;                                                                                                                         \
        if ((shade_delta) != 0)                                                                                                                                \
        {                                                                                                                                                      \
            actor->shade_step = (shade_delta);                                                                                                                 \
        }                                                                                                                                                      \
        actor->resource_index = 0;                                                                                                                             \
        if ((sequence_id) == 0)                                                                                                                                \
        {                                                                                                                                                      \
            actor->sequence = 0;                                                                                                                               \
        }                                                                                                                                                      \
        if ((shade_delta) == 0)                                                                                                                                \
        {                                                                                                                                                      \
            actor->shade_step = 0;                                                                                                                             \
        }                                                                                                                                                      \
        actor->target_shade = (shade_target);                                                                                                                  \
        actor->shade = (initial_shade);                                                                                                                        \
        (timer) = (frames);                                                                                                                                    \
        (step)++;                                                                                                                                              \
        next();                                                                                                                                                \
    }

#define WMAP_STEP_FADE_ACTOR(name, step, timer, actor_state, shade_delta, frames, next)                                                                        \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        WmapSpriteActor* actor = &(actor_state);                                                                                                               \
                                                                                                                                                               \
        actor->shade_step = (shade_delta);                                                                                                                     \
        actor->target_shade = 0;                                                                                                                               \
        (timer) = (frames);                                                                                                                                    \
        (step)++;                                                                                                                                              \
        next();                                                                                                                                                \
    }

#define WMAP_STEP_FADE_ACTOR_RANGE(name, step, timer, first, end, shade_delta, frames, next)                                                                   \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        s32 i;                                                                                                                                                 \
                                                                                                                                                               \
        for (i = (first); i < (end); i++)                                                                                                                      \
        {                                                                                                                                                      \
            g_wmap_sprite_actors[i].target_shade = 0;                                                                                                          \
            g_wmap_sprite_actors[i].shade_step = (shade_delta);                                                                                                \
        }                                                                                                                                                      \
        (timer) = (frames);                                                                                                                                    \
        (step)++;                                                                                                                                              \
        next();                                                                                                                                                \
    }

#define WMAP_INIT_PARTICLE_ACTOR(slot, animation_data, sequence_id)                                                                                            \
    {                                                                                                                                                          \
        WmapSpriteActor* actor;                                                                                                                                \
                                                                                                                                                               \
        g_wmap_actor_animations[(slot)].data = (animation_data);                                                                                               \
        actor = &g_wmap_sprite_actors[(slot)];                                                                                                                 \
        actor->resource_index = 0;                                                                                                                             \
        actor->scale_index = WMAP_SPRITE_SCALE_FULL;                                                                                                           \
        actor->sequence = (sequence_id);                                                                                                                       \
        actor->previous_sequence = -1;                                                                                                                         \
    }

#define WMAP_RESET_PARTICLE_SLOTS(index, count, motion_state, animation_first, animation_data)                                                                 \
    for ((index) = 0; (index) < (count); (index)++)                                                                                                            \
    {                                                                                                                                                          \
        (motion_state) = 0;                                                                                                                                    \
        g_wmap_actor_animations[(index) + (animation_first)].data = (animation_data);                                                                          \
    }

#define WMAP_STEP_STOP_EMITTER(name, step, timer, spawn_interval, frames, next)                                                                                \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        (timer) = (frames);                                                                                                                                    \
        (spawn_interval) = -1;                                                                                                                                 \
        (step)++;                                                                                                                                              \
        next();                                                                                                                                                \
    }

#define WMAP_STEP_STOP_PARTICLE_SPAWNS(name, step, timer, spawn_count, frames, next)                                                                           \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        (spawn_count) = 0;                                                                                                                                     \
        (timer) = (frames);                                                                                                                                    \
        (step)++;                                                                                                                                              \
        next();                                                                                                                                                \
    }

#define WMAP_STEP_ADVANCE(name, step)                                                                                                                          \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        step++;                                                                                                                                                \
    }

#define WMAP_STEP_WAIT_IDLE(name, step, next)                                                                                                                  \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        if (g_wmap_sequence_busy == 0)                                                                                                                         \
        {                                                                                                                                                      \
            step++;                                                                                                                                            \
            next();                                                                                                                                            \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_WAIT_SCROLL(name, step, next)                                                                                                                \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        if (g_wmap_view_scroll_mode != WMAP_SCROLL_SCRIPTED)                                                                                                   \
        {                                                                                                                                                      \
            (step)++;                                                                                                                                          \
            next();                                                                                                                                            \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_START_AND_WAIT(name, step, timer, runner, frames)                                                                                            \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        wmap_start_sequence(runner);                                                                                                                           \
        timer = frames;                                                                                                                                        \
        step++;                                                                                                                                                \
    }

#define WMAP_STEP_START_TWO_AND_WAIT(name, step, timer, first, second, frames)                                                                                 \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        wmap_start_sequence(first);                                                                                                                            \
        wmap_start_sequence(second);                                                                                                                           \
        timer = frames;                                                                                                                                        \
        step++;                                                                                                                                                \
    }

#define WMAP_STEP_START_THREE_AND_WAIT(name, step, timer, first, second, third, frames)                                                                        \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        wmap_start_sequence(first);                                                                                                                            \
        wmap_start_sequence(second);                                                                                                                           \
        wmap_start_sequence(third);                                                                                                                            \
        (timer) = (frames);                                                                                                                                    \
        (step)++;                                                                                                                                              \
    }

#define WMAP_STEP_HIDE_AND_WAIT(name, step, timer, hidden, frames)                                                                                             \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        (hidden) = 1;                                                                                                                                          \
        (timer) = (frames);                                                                                                                                    \
        (step)++;                                                                                                                                              \
    }

#define WMAP_STEP_HIDE_AND_START(name, step, timer, hidden, runner, frames)                                                                                    \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        (hidden) = 1;                                                                                                                                          \
        wmap_start_sequence(runner);                                                                                                                           \
        (timer) = (frames);                                                                                                                                    \
        (step)++;                                                                                                                                              \
    }

#define WMAP_STEP_ARM_TIMER(name, step, timer, frames, next)                                                                                                   \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        timer = frames;                                                                                                                                        \
        step++;                                                                                                                                                \
        next();                                                                                                                                                \
    }

#define WMAP_STEP_START_BLOCKING(name, step, runner, next)                                                                                                     \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        wmap_start_sequence(runner);                                                                                                                           \
        g_wmap_sequence_busy = 1;                                                                                                                              \
        step++;                                                                                                                                                \
        next();                                                                                                                                                \
    }

#define WMAP_STEP_DRAW_ACTOR_AND_WAIT(name, step, timer, actor, animation, position, texture_index, ot_index, variant)                                         \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        wmap_step_actor_animation(&actor, &animation);                                                                                                         \
        wmap_draw_actor_sprite(&actor, position.packed, texture_index, ot_index, variant);                                                                     \
        if (--timer == 0)                                                                                                                                      \
        {                                                                                                                                                      \
            step++;                                                                                                                                            \
        }                                                                                                                                                      \
    }

#define WMAP_FADE_OUT_PARTICLE_BURST(shade_step)                                                                                                               \
    {                                                                                                                                                          \
        s32 value;                                                                                                                                             \
                                                                                                                                                               \
        value = g_wmap_particle_shade - (shade_step);                                                                                                          \
        g_wmap_particle_shade = value;                                                                                                                         \
                                                                                                                                                               \
        if (value < 0)                                                                                                                                         \
        {                                                                                                                                                      \
            g_wmap_particle_shade = 0;                                                                                                                         \
        }                                                                                                                                                      \
                                                                                                                                                               \
        if (!(g_wmap_frame_count & 3))                                                                                                                         \
        {                                                                                                                                                      \
            g_wmap_particle_intensity--;                                                                                                                       \
        }                                                                                                                                                      \
                                                                                                                                                               \
        if (g_wmap_particle_intensity < 0)                                                                                                                     \
        {                                                                                                                                                      \
            g_wmap_particle_intensity = 0;                                                                                                                     \
        }                                                                                                                                                      \
    }

#define WMAP_MODEL_FADE_IN(shade, amount, maximum, result)                                                                                                     \
    {                                                                                                                                                          \
        (result) = (shade) + (amount);                                                                                                                         \
        (shade) = (result);                                                                                                                                    \
        if ((result) > (maximum))                                                                                                                              \
        {                                                                                                                                                      \
            (shade) = (maximum);                                                                                                                               \
        }                                                                                                                                                      \
    }

#define WMAP_MODEL_FADE_OUT(shade, amount, result)                                                                                                             \
    {                                                                                                                                                          \
        (result) = (shade) - (amount);                                                                                                                         \
        (shade) = (result);                                                                                                                                    \
        if ((result) < 0)                                                                                                                                      \
        {                                                                                                                                                      \
            (shade) = 0;                                                                                                                                       \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_START_MODEL(name, step, timer, rotation_copy, rotation, shade, initial_shade, frame_counter, frames, next)                                   \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        (shade) = (initial_shade);                                                                                                                             \
        (rotation_copy) = (rotation);                                                                                                                          \
        (frame_counter) = 0;                                                                                                                                   \
        (timer) = (frames);                                                                                                                                    \
        (step)++;                                                                                                                                              \
        next();                                                                                                                                                \
    }

#define WMAP_STEP_DROP_START(name, step, timer, rotation_copy, rotation, position, shade, shade_value, start_z, frames, next)                                  \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        rotation_copy = rotation;                                                                                                                              \
        position = g_wmap_camera_translation;                                                                                                                  \
        shade = shade_value;                                                                                                                                   \
        position.vz = start_z;                                                                                                                                 \
        timer = frames;                                                                                                                                        \
        step++;                                                                                                                                                \
        next();                                                                                                                                                \
    }

#define WMAP_DROP_MIN_Z 10000
#define WMAP_DROP_OT_INDEX 4
#define WMAP_DROP_TPAGE 0x35
#define WMAP_DROP_CLUT 0x7800
#define WMAP_DROP_BLEND_MODE 1

#define WMAP_STEP_DROP_UPDATE_WITH_DRAW(name, step, timer, rotation, position, shade, z_delta, shade_step, draw_call)                                          \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        MATRIX transform;                                                                                                                                      \
        s32 depth;                                                                                                                                             \
                                                                                                                                                               \
        depth = (position).vz + (z_delta);                                                                                                                     \
        (position).vz = depth;                                                                                                                                 \
        if (depth < WMAP_DROP_MIN_Z)                                                                                                                           \
        {                                                                                                                                                      \
            (position).vz = WMAP_DROP_MIN_Z;                                                                                                                   \
        }                                                                                                                                                      \
                                                                                                                                                               \
        PushMatrix();                                                                                                                                          \
        RotMatrix(&(rotation), &transform);                                                                                                                    \
        TransMatrix(&transform, &g_wmap_zero_translation);                                                                                                     \
        SetRotMatrix(&transform);                                                                                                                              \
        SetTransMatrix(&transform);                                                                                                                            \
                                                                                                                                                               \
        if ((shade) != 0)                                                                                                                                      \
        {                                                                                                                                                      \
            draw_call;                                                                                                                                         \
            (shade) -= (shade_step);                                                                                                                           \
            if ((shade) < 0)                                                                                                                                   \
            {                                                                                                                                                  \
                (shade) = 0;                                                                                                                                   \
            }                                                                                                                                                  \
        }                                                                                                                                                      \
                                                                                                                                                               \
        PopMatrix();                                                                                                                                           \
        if (--(timer) == 0)                                                                                                                                    \
        {                                                                                                                                                      \
            (step)++;                                                                                                                                          \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_DROP_UPDATE(name, step, timer, rotation, position, shade, resource, z_delta, shade_step)                                                     \
    WMAP_STEP_DROP_UPDATE_WITH_DRAW(                                                                                                                           \
        name, step, timer, rotation, position, shade, z_delta, shade_step,                                                                                     \
        wmap_draw_model_default((resource), 0, WMAP_DROP_OT_INDEX, WMAP_DROP_TPAGE, WMAP_DROP_CLUT, WMAP_DROP_BLEND_MODE, (shade)))

#define WMAP_STEP_MAP_DROP_UPDATE(name, step, timer, rotation, position, shade, resource, z_delta, shade_step)                                                 \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        (position).vz += (z_delta);                                                                                                                            \
        if ((position).vz < WMAP_DROP_MIN_Z)                                                                                                                   \
        {                                                                                                                                                      \
            (position).vz = WMAP_DROP_MIN_Z;                                                                                                                   \
        }                                                                                                                                                      \
                                                                                                                                                               \
        PushMatrix();                                                                                                                                          \
        wmap_set_map_rotation(&(rotation));                                                                                                                    \
                                                                                                                                                               \
        if ((shade) != 0)                                                                                                                                      \
        {                                                                                                                                                      \
            wmap_draw_model((resource), 0, WMAP_DROP_OT_INDEX, WMAP_DROP_TPAGE, WMAP_DROP_CLUT, WMAP_DROP_BLEND_MODE, (shade), 0, 0, -1);                      \
            (shade) -= (shade_step);                                                                                                                           \
            if ((shade) < 0)                                                                                                                                   \
            {                                                                                                                                                  \
                (shade) = 0;                                                                                                                                   \
            }                                                                                                                                                  \
        }                                                                                                                                                      \
                                                                                                                                                               \
        PopMatrix();                                                                                                                                           \
        if (--(timer) == 0)                                                                                                                                    \
        {                                                                                                                                                      \
            (step)++;                                                                                                                                          \
        }                                                                                                                                                      \
    }

#define WMAP_STEP_FINISH_BLOCKING(name, step)                                                                                                                  \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        g_wmap_sequence_busy = 0;                                                                                                                              \
        (step)++;                                                                                                                                              \
    }

#define WMAP_STEP_BEGIN_EXIT(name, step, mode)                                                                                                                 \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        g_wmap_exit_frame = 1;                                                                                                                                 \
        g_wmap_exit_mode = (mode);                                                                                                                             \
        (step)++;                                                                                                                                              \
    }

#define WMAP_STEP_FINISH_MARK_CELL(name, step, cells, cell_x, cell_y, cell_value)                                                                              \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        g_wmap_sequence_busy = 0;                                                                                                                              \
        cells[cell_x][cell_y].land_id = cell_value | WMAP_LAND_FULL_SIZE;                                                                                      \
        step++;                                                                                                                                                \
    }

/**
 * @brief Turn a model by one rotation step and count down its timer.
 * @param rotation Model rotation; its z angle advances.
 * @param rotation_step Angle added this frame.
 * @param timer Frames left; decremented.
 * @return Frames left after this one.
 */
static inline s32 wmap_tick_model_motion(SVECTOR* rotation, s32 rotation_step, s32* timer)
{
    s32 remaining;

    rotation->vz += rotation_step;
    remaining = *timer - 1;
    *timer = remaining;
    return remaining;
}

#endif
