#include "lom_native_graphics.h"
#include "lom_native_boundary.h"

/**
 * @brief Pack two 16-bit numbers into one word.
 * @param low The low half.
 * @param high The high half.
 * @return The word.
 */
static unsigned pack(int low, int high)
{
    return (unsigned)(low & 0xFFFF) | (unsigned)(high & 0xFFFF) << 16;
}

/**
 * @brief Report a graphics call to lom_native_boundary.h.
 * @param call Which call.
 * @param area The VRAM it touches, or NULL.
 */
static void report(LomGraphicsCall call, const RECT* area)
{
    if (area)
    {
        lom_native_boundary_event(LOM_BOUNDARY_GRAPHICS, call, pack(area->x, area->y), pack(area->w, area->h));
    }
    else
    {
        lom_native_boundary_event(LOM_BOUNDARY_GRAPHICS, call, 0, 0);
    }
}

int lom_graphics_upload(RECT* area, const void* pixels)
{
    report(LOM_GRAPHICS_UPLOAD, area);
    return LoadImage(area, (u_long*)pixels);
}

int lom_graphics_download(RECT* area, void* pixels)
{
    report(LOM_GRAPHICS_DOWNLOAD, area);
    return StoreImage(area, (u_long*)pixels);
}

int lom_graphics_copy(RECT* area, int x, int y)
{
    report(LOM_GRAPHICS_COPY, area);
    return MoveImage(area, x, y);
}

int lom_graphics_copy_now(RECT* area, int x, int y)
{
    report(LOM_GRAPHICS_COPY, area);
    return MoveImage2(area, x, y);
}

int lom_graphics_fill(RECT* area, int r, int g, int b)
{
    report(LOM_GRAPHICS_FILL, area);
    return ClearImage(area, (u_char)r, (u_char)g, (u_char)b);
}

int lom_graphics_wait(int mode)
{
    return DrawSync(mode);
}

DISPENV* lom_graphics_set_display(DISPENV* display)
{
    report(LOM_GRAPHICS_SET_DISPLAY, display ? &display->disp : 0);
    return PutDispEnv(display);
}

DRAWENV* lom_graphics_set_drawing(DRAWENV* drawing)
{
    report(LOM_GRAPHICS_SET_DRAWING, drawing ? &drawing->clip : 0);
    return PutDrawEnv(drawing);
}

DISPENV* lom_graphics_default_display(DISPENV* display, int x, int y, int w, int h)
{
    return SetDefDispEnv(display, x, y, w, h);
}

DRAWENV* lom_graphics_default_drawing(DRAWENV* drawing, int x, int y, int w, int h)
{
    return SetDefDrawEnv(drawing, x, y, w, h);
}

void lom_graphics_show(int on)
{
    lom_native_boundary_event(LOM_BOUNDARY_GRAPHICS, LOM_GRAPHICS_SHOW, (unsigned)on, 0);
    SetDispMask(on);
}

int lom_graphics_reset(int mode)
{
    lom_native_boundary_event(LOM_BOUNDARY_GRAPHICS, LOM_GRAPHICS_RESET, (unsigned)mode, 0);
    return ResetGraph(mode);
}

void* lom_graphics_clear_list(void* list, int count)
{
    return ClearOTagR((OT_TYPE*)list, count);
}

void lom_graphics_draw_packet(void* packet)
{
    report(LOM_GRAPHICS_DRAW_PACKET, 0);
    DrawPrim(packet);
}
