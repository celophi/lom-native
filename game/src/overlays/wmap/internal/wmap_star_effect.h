#ifndef WMAP_STAR_EFFECT_H
#define WMAP_STAR_EFFECT_H

#include "wmap_sequence_runtime.h"
#include "wmap_sprite_render.h"
#include <inline_c.h>
#include "sdk/gte_dmpsx_compat.h"

typedef struct
{
    s16 state;
    s16 angle;
    u16 angle_step;
    s16 unused_06;
    s32 radius;
    s16 unused_0c;
    u16 depth;
    s16 unused_10;
    s16 unused_12;
} WmapOrbitingStar;

#define WMAP_STAR_OPERAND_SHIFT 6
#define WMAP_STAR_PRODUCT_SHIFT 12
#define WMAP_STAR_ANGLE_MASK 0xFFF

#define WMAP_DEFINE_STAR_UPDATE(name, step, timer, first, end, texture_index)                                                                                  \
    void name(void)                                                                                                                                            \
    {                                                                                                                                                          \
        SVECTOR position;                                                                                                                                      \
        s32 screen;                                                                                                                                            \
        WmapOrbitingStar* star;                                                                                                                                \
        WmapSpriteActor* actor;                                                                                                                                \
        s32 i;                                                                                                                                                 \
                                                                                                                                                               \
        for (i = (first); i < (end); i++)                                                                                                                      \
        {                                                                                                                                                      \
            actor = &g_wmap_sprite_actors[i];                                                                                                                  \
            star = (WmapOrbitingStar*)&g_wmap_actor_motions[i];                                                                                                \
            position.vx = ((star->radius >> WMAP_STAR_OPERAND_SHIFT) * (ccos(star->angle) >> WMAP_STAR_OPERAND_SHIFT)) >> WMAP_STAR_PRODUCT_SHIFT;             \
            position.vy = ((star->radius >> WMAP_STAR_OPERAND_SHIFT) * (csin(star->angle) >> WMAP_STAR_OPERAND_SHIFT)) >> WMAP_STAR_PRODUCT_SHIFT;             \
            position.vz = star->depth;                                                                                                                         \
            gte_ldv0(&position);                                                                                                                               \
            gte_rtps();                                                                                                                                        \
            wmap_step_actor_animation(actor, &g_wmap_actor_animations[i]);                                                                                     \
            gte_stsxy(&screen);                                                                                                                                \
            if (star->angle != 0)                                                                                                                              \
            {                                                                                                                                                  \
                wmap_draw_actor_sprite(actor, screen, (texture_index), 4, 0);                                                                                  \
            }                                                                                                                                                  \
            else                                                                                                                                               \
            {                                                                                                                                                  \
                wmap_draw_actor_sprite(actor, screen, (texture_index), 0, 0);                                                                                  \
            }                                                                                                                                                  \
            star->angle = ((u16)star->angle + star->angle_step) & WMAP_STAR_ANGLE_MASK;                                                                        \
        }                                                                                                                                                      \
        if (--(timer) == 0)                                                                                                                                    \
        {                                                                                                                                                      \
            (step)++;                                                                                                                                          \
        }                                                                                                                                                      \
    }

#endif
