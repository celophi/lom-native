#ifndef WMAP_MAP_LABELS_H
#define WMAP_MAP_LABELS_H

#include "common.h"

void wmap_draw_status_panel(void);
void wmap_draw_map_labels(void);
void wmap_set_map_label(s32 selection);
void wmap_init_label_sprites(void);

extern s32 g_wmap_auxiliary_label_mode;

extern s32 g_wmap_auxiliary_label_draw_mode;

extern s32 g_wmap_land_label_updates_enabled;

extern s16 g_wmap_land_label_cells[];

#endif
