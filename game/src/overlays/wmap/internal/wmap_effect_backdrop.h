#ifndef WMAP_EFFECT_BACKDROP_H
#define WMAP_EFFECT_BACKDROP_H

#include "common.h"

extern s32 g_wmap_mesh_transition_frames;
extern s32 g_wmap_mesh_transition_direction;
extern s32 g_wmap_mesh_transition_selection;
extern s32 g_wmap_mesh_previous_selection;

void wmap_init_transition_mesh(void);
void wmap_draw_transition_mesh(void);
void wmap_start_mesh_transition(s32 selection);
void wmap_select_mesh_motion();

#endif
