#ifndef _PAD_H
#define _PAD_H

#include "common.h"

typedef enum
{
    PAD_BTN_L2 = 0x0001,
    PAD_BTN_R2 = 0x0002,
    PAD_BTN_L1 = 0x0004,
    PAD_BTN_R1 = 0x0008,
    PAD_BTN_SQUARE = 0x0010,
    PAD_BTN_CROSS = 0x0020,
    PAD_BTN_CIRCLE = 0x0040,
    PAD_BTN_TRIANGLE = 0x0080,
    PAD_BTN_SELECT = 0x0100,
    PAD_BTN_L3 = 0x0200,
    PAD_BTN_R3 = 0x0400,
    PAD_BTN_START = 0x0800,
    PAD_BTN_UP = 0x1000,
    PAD_BTN_RIGHT = 0x2000,
    PAD_BTN_DOWN = 0x4000,
    PAD_BTN_LEFT = 0x8000
} PadButton;

#define PAD_BTN_DPAD (PAD_BTN_UP | PAD_BTN_RIGHT | PAD_BTN_DOWN | PAD_BTN_LEFT)

#define PAD_CONFIRM_BUTTONS (PAD_BTN_CROSS | PAD_BTN_L3)

#define PAD_DISMISS_BUTTONS (PAD_CONFIRM_BUTTONS | PAD_BTN_CIRCLE)

#define PAD_REPEAT_INITIAL_DELAY 15

#define PAD_REPEAT_INTERVAL 2

#define PAD_REPEAT_HOLD_BUTTONS                                                                                                                                \
    (PAD_BTN_L2 | PAD_BTN_R2 | PAD_BTN_L1 | PAD_BTN_R1 | PAD_BTN_CROSS | PAD_BTN_CIRCLE | PAD_BTN_SELECT | PAD_BTN_L3 | PAD_BTN_START)

#define PAD_STICK_THRESHOLD 1

#define PAD_ALL_BUTTONS 0xFFFF

typedef union
{
    u16 raw;
    s16 signed_value;
} PadAxisSample;

typedef struct
{
    u8 device_type;
    u8 _pad1;
    volatile u16 held_buttons;
    u16 pressed_buttons;

    u8 _pad2[0x28 - 2];

    PadAxisSample axis_x;
    PadAxisSample axis_y;
} SCDRegs;

#if defined(LOM_NATIVE_POINTERS)
#include "lom_native_input.h"
#define SCD_REGS ((SCDRegs*)lom_input_ports())
#else
#define SCD_REGS ((SCDRegs*)CONTROLLER_STATE_ADDRESS)
#endif

#if defined(LOM_NATIVE_POINTERS)
#define g_controller_device_type (SCD_REGS->device_type)
#else
extern u8 g_controller_device_type;
#endif

#define PAD_SWAP_BYTES(b) (((b) << 8) | ((b) >> 8))

#define PAD_REMAP_FACE_BITS(b)                                                                                                                                 \
    ((((b) & PAD_BTN_CIRCLE) >> 1) | (((b) & PAD_BTN_CROSS) << 1) | (((b) & PAD_BTN_TRIANGLE) >> 3) | (((b) & PAD_BTN_SQUARE) << 3) | ((b) & ~0xF0))

#define PAD_REMAP_FACE_BITS_16(b)                                                                                                                              \
    (((u32)((b) & PAD_BTN_CIRCLE) >> 1) | (((b) & PAD_BTN_CROSS) << 1) | ((u32)((b) & PAD_BTN_TRIANGLE) >> 3) | (((b) & PAD_BTN_SQUARE) << 3) |                \
     ((b) & (u16)~0xF0))

#endif
