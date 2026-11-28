#include "lom_native_draw.h"
#include "lom_native_boundary.h"
#include "lom_native_graphics.h"
#include "lom_native_remaster.h"
#include "lom_native_cd.h"

void lom_draw_field_sprite(void* list, void* quad, int resource, const void* frame, int x, int y, int z)
{
    (void)frame;
    lom_native_boundary_event(LOM_BOUNDARY_GRAPHICS, LOM_GRAPHICS_DRAW_FIELD_SPRITE, (unsigned)resource, (unsigned)(x & 0xFFFF) | (unsigned)(z & 0xFFFF) << 16);
    (void)y;
    addPrim(list, quad);
}

void lom_draw_field_tiles(void* list, void* first, void* last, const void* part, int shade)
{
    (void)part;
    lom_native_boundary_event(LOM_BOUNDARY_GRAPHICS, LOM_GRAPHICS_DRAW_FIELD_TILES, (unsigned)shade, 0);
    addPrims(list, first, last);
}

void lom_draw_field_map_ready(const unsigned short* lookup)
{
    lom_remaster_map_ready(lookup);
}

void lom_draw_field_tile_record(const void* desc, const void* record)
{
    lom_remaster_tile_record(desc, record);
}

int lom_draw_field_layer_shift(int scale, int camera_x, int room_width, int room_depth)
{
    return lom_remaster_layer_shift(scale, camera_x, room_width, room_depth);
}

int lom_draw_field_room_extent(int room_width, int room_depth, int* left, int* right)
{
    return lom_remaster_room_extent(room_width, room_depth, left, right);
}

void lom_draw_field_tile_copied(const void* to, const void* from)
{
    lom_remaster_tile_copied(to, from);
}

void lom_draw_field_tile(void* code, const void* record)
{
    lom_remaster_tile(code, record);
}

void lom_draw_encyclopedia_picture(void* code, int stride, int count, int resource)
{
    lom_remaster_encyclopedia_picture(code, stride, count, lom_native_cd_resource_name(resource));
}

void lom_draw_wsel_loaded(void)
{
    lom_remaster_wsel_loaded();
}

void lom_draw_wsel_picture(void* code, int stride, int count, int picture, int shadow, int x, int y)
{
    lom_remaster_wsel_picture(code, stride, count, picture, shadow, x, y);
}

void lom_draw_title_backdrop(void* code, int stride, int count)
{
    lom_remaster_title_backdrop(code, stride, count);
}

void lom_draw_title_menu(void* code, int part, int index, int selected)
{
    lom_remaster_title_menu(code, part, index, selected);
}

void lom_draw_cards_begin(void)
{
    lom_remaster_cards_begin();
}

void lom_draw_cards_end(void)
{
    lom_remaster_cards_end();
}

int lom_draw_cards_on(void)
{
    return lom_remaster_cards_on();
}

void lom_draw_cards_frame(void)
{
    lom_remaster_cards_frame();
}

void lom_draw_cards_background(void* code, int stride, int count)
{
    lom_remaster_cards_background(code, stride, count);
}

void lom_draw_cards_picture(void* code, int picture, int x, int y, int w, int h)
{
    lom_remaster_cards_picture(code, picture, x, y, w, h);
}

int lom_draw_cards_text(void* code, const unsigned char* text, int x, int baseline, int align, int style)
{
    return lom_remaster_cards_text(code, text, x, baseline, align, style);
}

void lom_draw_gname_background(void* code, int stride, int count)
{
    lom_remaster_gname_background(code, stride, count);
}

void lom_draw_gname_glyph(void* code, int sprites, int stride, int glyph)
{
    lom_remaster_gname_glyph(code, sprites, stride, glyph);
}

int lom_draw_gname_column(void)
{
    return lom_remaster_gname_layout();
}

int lom_draw_gname_tab_shift(void)
{
    return lom_remaster_gname_tab_shift();
}

void lom_draw_gname_cursor(int position, int* x, int* y)
{
    lom_remaster_gname_cursor(position, x, y);
}

void lom_draw_menu_begin(void)
{
    lom_remaster_menu_begin();
}

void lom_draw_menu_end(void)
{
    lom_remaster_menu_end();
}

void lom_draw_menu_frame(void)
{
    lom_remaster_menu_frame();
}

int lom_draw_menu_columns(void)
{
    return lom_remaster_menu_columns();
}

void lom_draw_menu_tab_xy(int column, int row, int* x, int* y)
{
    lom_remaster_menu_tab_xy(column, row, x, y);
}

void lom_draw_menu_background(void* code, int stride, int count)
{
    lom_remaster_menu_background(code, stride, count);
}

void lom_draw_menu_icon(void* code, int sprites, int stride, int icon, int mirror)
{
    lom_remaster_menu_icon(code, sprites, stride, icon, mirror);
}

void lom_draw_menu_key(void* code, int stride, int x, int y, int w, int h, const char* name)
{
    lom_remaster_menu_key(code, stride, x, y, w, h, name);
}

const unsigned char* lom_draw_menu_prompt(int which)
{
    return lom_remaster_menu_prompt(which);
}

void lom_draw_wsel_zoom(void* code)
{
    lom_remaster_wsel_zoom(code);
}

void lom_draw_wmap_status(void* code, int part)
{
    lom_remaster_wmap_status(code, part);
}

void lom_draw_wmap_label(void* code, int selection, int tpage)
{
    lom_remaster_wmap_label(code, selection, tpage);
}

void lom_draw_wmap_panel(void* code, int stage)
{
    lom_remaster_wmap_panel(code, stage);
}

void lom_draw_wmap_spirit(void* code, int spirit, int kind)
{
    lom_remaster_wmap_spirit(code, spirit, kind);
}
