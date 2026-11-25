#ifndef _MAIN_H
#define _MAIN_H

#include "common.h"
#include "main/game_state.h"
#include "common/saved_game.h"
#include "main/field_entry.h"
#include <libgte.h>
#include <libgpu.h>
#include <libapi.h>
#include <libetc.h>

extern s32 g_script_unused_value;

extern s32 g_field_audio_timer;
extern s32 g_save_compatibility_tag;

extern s32 g_frame_counter;

extern u8 g_field_party_portraits[];

extern s32 g_script_repeat_count;

extern s32 g_active_script;

extern s32 g_pad_input;

extern s32 g_pad_input_inject;

/** @brief Initialize the game and dispatch overlays forever. */
void main_game_loop(void);

#include "main/main_steps.h"

#endif
