#ifndef _GPU_PACKET_H
#define _GPU_PACKET_H

#include "common.h"

#define SET_YX0(p, _y0, _x0) (p)->y0 = (_y0), (p)->x0 = (_x0)

#define SET_BGR0(p, _b0, _g0, _r0) (p)->b0 = _b0, (p)->g0 = _g0, (p)->r0 = _r0

#define SET_BGR0_PACKED(p, _word) (*(u32*)((u8*)(p) + __builtin_offsetof(P_TAG, r0)) = (u32)(_word))

#define GPU_COLOR_WORD(_r, _g, _b) (((u32)(_b) << 16) | ((u32)(_g) << 8) | (u32)(_r))

#define GPU_TINT_NEUTRAL_LEVEL 0x80

#define GPU_TINT_NEUTRAL GPU_COLOR_WORD(GPU_TINT_NEUTRAL_LEVEL, GPU_TINT_NEUTRAL_LEVEL, GPU_TINT_NEUTRAL_LEVEL)

#define GPU_COLOR_WHITE GPU_COLOR_WORD(0xFF, 0xFF, 0xFF)
#define GPU_COLOR_BLACK GPU_COLOR_WORD(0, 0, 0)

#define GPU_COLOR_SELECTED_ROW GPU_COLOR_WORD(0xF0, 0x80, 0xF0)

#define SET_POLY_G4_BGR1_PACKED(p, _word) (*(u32*)&(p)->r1 = (u32)(_word))
#define SET_POLY_G4_BGR2_PACKED(p, _word) (*(u32*)&(p)->r2 = (u32)(_word))
#define SET_POLY_G4_BGR3_PACKED(p, _word) (*(u32*)&(p)->r3 = (u32)(_word))

#define SET_XY0_PACKED(p, _xy) (*(u32*)&(p)->x0 = (u32)(_xy))
#define SET_XY1_PACKED(p, _xy) (*(u32*)&(p)->x1 = (u32)(_xy))
#define SET_XY2_PACKED(p, _xy) (*(u32*)&(p)->x2 = (u32)(_xy))
#define SET_XY3_PACKED(p, _xy) (*(u32*)&(p)->x3 = (u32)(_xy))

#define SET_UV0_PACKED(p, _uv) (*(u16*)&(p)->u0 = (_uv))
#define SET_UV1_PACKED(p, _uv) (*(u16*)&(p)->u1 = (_uv))
#define SET_UV2_PACKED(p, _uv) (*(u16*)&(p)->u2 = (_uv))
#define SET_UV3_PACKED(p, _uv) (*(u16*)&(p)->u3 = (_uv))

#define SET_SPRT_XY0_WORD(p, _xy) (*(u32*)((u8*)(p) + __builtin_offsetof(SPRT, x0)) = (u32)(_xy))

#define SET_SPRT_WH_WORD(p, _wh) (*(u32*)((u8*)(p) + __builtin_offsetof(SPRT, w)) = (u32)(_wh))

#define SET_SPRT_WH_PACKED(p, _w, _h) (*(u32*)((u8*)(p) + __builtin_offsetof(SPRT, w)) = ((u32)(u16)(_h) << 16) | (u32)(u16)(_w))

#define SET_SPRT_UV0_PACKED(p, _uv) (*(u16*)((u8*)(p) + __builtin_offsetof(SPRT, u0)) = (u16)(_uv))

#define SET_SPRT_UV_CLUT_WORD(p, _word) (*(u32*)((u8*)(p) + __builtin_offsetof(SPRT, u0)) = (u32)(_word))

#define SET_SPRT_CLUT(p, _clut) (*(u16*)((u8*)(p) + __builtin_offsetof(SPRT, clut)) = (u16)(_clut))

#define GPU_CODE_POLY_FT3 0x24
#define GPU_CODE_POLY_F4 0x28
#define GPU_CODE_POLY_FT4 0x2C
#define GPU_CODE_POLY_G4 0x38
#define GPU_CODE_LINE_G2 0x50
#define GPU_CODE_TILE 0x60
#define GPU_CODE_SPRT 0x64
#define GPU_CODE_SPRT_16 0x7C
#define GPU_CODE_SEMI_TRANS 0x02
#define GPU_CODE_RAW_TEXTURE 0x01

#define GPU_TEXTURE_4BIT 0
#define GPU_TEXTURE_8BIT 1
#define GPU_TEXTURE_16BIT 2

#define GPU_CLUT_4BIT_COLORS 16
#define GPU_CLUT_8BIT_COLORS 256

#define GPU_COLOR15_CHANNEL_MASK 0x1F
#define GPU_COLOR15_GREEN_SHIFT 5
#define GPU_COLOR15_BLUE_SHIFT 10
#define GPU_STP_BIT 0x8000

#define GPU_BLEND_HALF 0
#define GPU_BLEND_ADD 1
#define GPU_BLEND_SUBTRACT 2
#define GPU_BLEND_ADD_QUARTER 3

#define GPU_TPAGE_BLEND_SHIFT 5
#define GPU_TPAGE_BLEND_MASK (3 << GPU_TPAGE_BLEND_SHIFT)

#define PRIM_WORDS(T) (sizeof(T) / sizeof(u_long))

#if defined(LOM_NATIVE_POINTERS)
#define GPU_PACKET_TAG(type) O_TAG
#else
#define GPU_PACKET_TAG(type) type tag
#endif

#if defined(LOM_NATIVE_POINTERS)
#define GPU_PS1_PACKET_SIZE(T)                                                                                                                                 \
    _Generic((__typeof__(T)*)0,                                                                                                                                \
        POLY_F3*: 0x14,                                                                                                                                        \
        POLY_F4*: 0x18,                                                                                                                                        \
        POLY_FT3*: 0x20,                                                                                                                                       \
        POLY_FT4*: 0x28,                                                                                                                                       \
        POLY_G3*: 0x1C,                                                                                                                                        \
        POLY_G4*: 0x24,                                                                                                                                        \
        POLY_GT3*: 0x28,                                                                                                                                       \
        POLY_GT4*: 0x34,                                                                                                                                       \
        LINE_F2*: 0x10,                                                                                                                                        \
        LINE_G2*: 0x14,                                                                                                                                        \
        SPRT*: 0x14,                                                                                                                                           \
        SPRT_8*: 0x10,                                                                                                                                         \
        SPRT_16*: 0x10,                                                                                                                                        \
        TILE*: 0x10)
#else
#define GPU_PS1_PACKET_SIZE(T) sizeof(T)
#endif

#if defined(LOM_NATIVE_POINTERS)
#define GPU_OT_ENTRY(type) OT_TYPE
#else
#define GPU_OT_ENTRY(type) type
#endif

#endif
