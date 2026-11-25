#include "main/overlay_memory.h"

#if defined(LOM_NATIVE_POINTERS)
void* get_overlay_load_base(void)
{
    return (void*)OVERLAY_LOAD_BASE_ADDRESS;
}

void* get_field_render_buffers(void)
{
    return (void*)FIELD_RENDER_BUFFERS_ADDRESS;
}

void* get_world_map_overlay_end(void)
{
    return (void*)WORLD_MAP_OVERLAY_END_ADDRESS;
}

void* get_title_menu_buffers(void)
{
    return (void*)TITLE_OVERLAY_END_ADDRESS;
}
#else
extern u8 g_field_render_buffers;
extern u8 g_world_map_overlay_end;
extern u8 g_title_overlay_end;

/**
 * @brief Return the common base address used for loading overlays.
 * @return First byte past the main executable image, where overlays are loaded.
 */
void* get_overlay_load_base(void)
{
    return &g_overlay_load_base;
}

/**
 * @brief Return the render-buffer base immediately after the FIELD overlay image.
 * @return Base of the two field render buffers.
 * @see decomp.me (100%) https://decomp.me/scratch/rgamP
 */
void* get_field_render_buffers(void)
{
    return &g_field_render_buffers;
}

/**
 * @brief Return the address immediately after the decompressed WMAP overlay image.
 * @return First byte past the WMAP overlay image.
 * @see decomp.me (100%) https://decomp.me/scratch/B5ptQ
 */
void* get_world_map_overlay_end(void)
{
    return &g_world_map_overlay_end;
}

/**
 * @brief Return the menu-buffer base immediately after the TITLE overlay image.
 * @return Base of the title menu buffers.
 * @see decomp.me (100%) https://decomp.me/scratch/fl1lB
 */
void* get_title_menu_buffers(void)
{
    return &g_title_overlay_end;
}
#endif
