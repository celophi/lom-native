#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "common.h"

#define GAME_STATE_FIELD 0
#define GAME_STATE_WORLD_MAP 1
#define GAME_STATE_TITLE 2
#define GAME_STATE_GNAME 3
#define GAME_STATE_RETURN_TO_TITLE 4
#define GAME_STATE_WORLD_SELECT 5
#define GAME_STATE_CHECKPS 6
#define GAME_STATE_MENU_LOAD 7
#define GAME_STATE_INTRO_MOVIE 8
#define GAME_STATE_ATTRACT_1 9
#define GAME_STATE_ATTRACT_2 10
#define GAME_STATE_NONE 0xFF

extern u32 g_game_state;
extern s32 g_pending_game_state;
extern u32 g_previous_game_state;

extern u16 g_current_land;

#endif
