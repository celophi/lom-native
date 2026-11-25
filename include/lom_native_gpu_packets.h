#ifndef LOM_NATIVE_GPU_PACKETS_H
#define LOM_NATIVE_GPU_PACKETS_H

#define lom_gpu_link addPrim
#define lom_gpu_link_run addPrims
#define lom_gpu_set_next setaddr
#define lom_gpu_next getaddr
#define lom_gpu_set_length setlen
#define lom_gpu_set_code setcode
#define lom_gpu_set_semi_trans setSemiTrans
#define lom_gpu_init_sprite setSprt
#define lom_gpu_init_sprite16 setSprt16
#define lom_gpu_init_tile setTile
#define lom_gpu_init_poly_f3 setPolyF3
#define lom_gpu_init_poly_f4 setPolyF4
#define lom_gpu_init_poly_ft3 setPolyFT3
#define lom_gpu_init_poly_ft4 setPolyFT4
#define lom_gpu_prepare_poly_ft4 SetPolyFT4
#define lom_gpu_init_poly_g3 setPolyG3
#define lom_gpu_init_poly_g4 setPolyG4
#define lom_gpu_prepare_poly_g4 SetPolyG4
#define lom_gpu_init_line_f2 setLineF2
#define lom_gpu_init_line_f4 setLineF4
#define lom_gpu_init_line_g2 setLineG2
#define lom_gpu_init_tpage_packet setDrawTPage
#define lom_gpu_init_drawing_packet SetDrawEnv
#define lom_gpu_init_texture_window setTexWindow
#define lom_gpu_set_xy0 setXY0
#define lom_gpu_set_xy2 setXY2
#define lom_gpu_set_xy4 setXY4
#define lom_gpu_set_uv0 setUV0
#define lom_gpu_set_uv4 setUV4
#define lom_gpu_set_wh setWH
#define lom_gpu_set_rgb0 setRGB0
#define lom_gpu_set_clut setClut
#define lom_gpu_set_tpage setTPage
#define lom_gpu_set_rect setRECT
#define lom_gpu_clut getClut
#define lom_gpu_tpage getTPage
#define lom_gpu_mode _get_mode

#endif
