#ifndef GLYPH_CACHE_H
#define GLYPH_CACHE_H

#include "common.h"
#include <libgpu.h>
#include <libgte.h>

#include "common/encoded_text.h"
#include "common/glyph_format.h"
#include "common/sjis.h"
#include "overlays/field/field_text.h"

typedef union
{
    u32 raw;

    struct
    {
        u16 code;
        u16 flags;
    } data;
} GlyphCacheEntry;

typedef struct
{
    SPRT_16 packet;
    u32 padding;
} GlyphSprite;

extern GlyphCacheEntry g_glyph_cache[GLYPH_CACHE_SLOTS];
extern u8 g_glyph_raster_buffer[GLYPH_RASTER_BUFFER_BYTES];
extern u8* g_glyph_raster_cursor;
extern s32 g_glyph_cursor_x;
extern s32 g_glyph_cursor_y;
extern s32 g_glyph_line_start_x;
extern s32 g_glyph_upload_x;
extern s32 g_glyph_upload_y;

extern u8 g_glyph_single_byte_chart[];

extern u8 g_glyph_chart_page_base[];

#ifndef GLYPH_CHART_PAGE_BASE
#define GLYPH_CHART_PAGE_BASE g_glyph_chart_page_base
#endif

extern u16 g_glyph_decimal_digits[];
extern u16 g_glyph_hex_digits[];

void* draw_signed_decimal(void* prim, OT_TYPE* ot, s32 value, s32 x, s32 y, s32 palette, s32 alignment);
void draw_hex_byte(void* prim, OT_TYPE* ot, s32 value, s32 x, s32 y, s32 alignment);
void* draw_cached_text(void* prim, OT_TYPE* ot, u8* text, s32 x, s32 y, s32 palette, s32 alignment);
void* render_cached_glyph(void* primitive, OT_TYPE* ot_tag, u16 character_code, s32 palette);
void* emit_glyph_sprite(GlyphSprite* sprite, OT_TYPE* ot, s32 cache_slot, s32 palette);
void begin_glyph_cache_frame(void);
void evict_unused_glyphs(void);
void reset_glyph_cache(void);
void expand_text_glyph_codes(u8* destination, u8* source);

#endif
