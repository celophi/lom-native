#ifndef FIELD_WORKSPACE_H
#define FIELD_WORKSPACE_H

#include "common.h"

#include "main/display.h"
#include "overlays/field/field_text.h"

typedef struct
{
    u8* top;
    FieldTextConfig* text_configs;
    u8* timed_text;
    u8* primitive_buffers[DISPLAY_BUFFER_COUNT];
    u32 draw_mode0;
    u32 draw_mode1;
    u16 text_clut;
    u16 text_alt_clut;
    u16 window_clut;
    u16 prompt_clut;
    u16 portrait_clut[2];
    u16 portrait_slots;
    u16 _pad2A;
    s32 fade_mode;
    s32 fade_level;
    FieldTextState windows[FIELD_TEXT_WINDOW_SLOTS];
} FieldWorkspace;

#define FIELD_WORKSPACE ((FieldWorkspace*)FIELD_WORKSPACE_ADDRESS)

#endif
