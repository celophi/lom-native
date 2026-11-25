#if defined(VERSION_JP)
#define GLYPH_CHART_PAGE_BASE ((u8*)cload_draw_backdrop + 0x70)
#else
#define GLYPH_CHART_PAGE_BASE ((u8*)cload_load_icon_resources + 0x2C)
#endif

#include "internal/cload_internal.h"
#include "lom_native_card.h"

#include "../../common/glyph_cache/draw_signed_decimal.inc.c"
#include "../../common/glyph_cache/draw_hex_byte.inc.c"
#include "../../common/glyph_cache/draw_cached_text.inc.c"
#include "../../common/glyph_cache/render_cached_glyph.inc.c"
#include "../../common/glyph_cache/emit_glyph_sprite.inc.c"
#include "../../common/glyph_cache/begin_glyph_cache_frame.inc.c"
#include "../../common/glyph_cache/evict_unused_glyphs.inc.c"
#include "../../common/glyph_cache/reset_glyph_cache.inc.c"
#include "../../common/glyph_cache/expand_text_glyph_codes.inc.c"
