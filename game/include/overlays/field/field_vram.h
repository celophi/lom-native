#ifndef FIELD_VRAM_H
#define FIELD_VRAM_H
#include "lom_native_gpu_packets.h"

#define FIELD_UI_PAGE_X 960
#define FIELD_UI_PAGE_Y 256
#define FIELD_UI_TPAGE(blend) lom_gpu_tpage(GPU_TEXTURE_4BIT, (blend), FIELD_UI_PAGE_X, FIELD_UI_PAGE_Y)

#define FIELD_UI_CLUT_X 256
#define FIELD_UI_CLUT_Y 480

#endif
