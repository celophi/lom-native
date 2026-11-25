#ifndef LOM_NATIVE_SDK_GTE_DMPSX_COMPAT_H
#define LOM_NATIVE_SDK_GTE_DMPSX_COMPAT_H

#include <libgte.h>

#undef gte_rtps
#define gte_rtps() Psyz_GteCommand(0x0180001)
#undef gte_rtpt
#define gte_rtpt() Psyz_GteCommand(0x0280030)
#undef gte_rtv0
#define gte_rtv0() Psyz_GteCommand(0x0486012)
#undef gte_rtv1
#define gte_rtv1() Psyz_GteCommand(0x048e012)
#undef gte_rtv2
#define gte_rtv2() Psyz_GteCommand(0x0496012)
#undef gte_rtir
#define gte_rtir() Psyz_GteCommand(0x049e012)
#undef gte_rtir_sf0
#define gte_rtir_sf0() Psyz_GteCommand(0x041e012)
#undef gte_rtv0tr
#define gte_rtv0tr() Psyz_GteCommand(0x0480012)
#undef gte_rtv1tr
#define gte_rtv1tr() Psyz_GteCommand(0x0488012)
#undef gte_rtv2tr
#define gte_rtv2tr() Psyz_GteCommand(0x0490012)
#undef gte_rtirtr
#define gte_rtirtr() Psyz_GteCommand(0x0498012)
#undef gte_rtv0bk
#define gte_rtv0bk() Psyz_GteCommand(0x0482012)
#undef gte_rtv1bk
#define gte_rtv1bk() Psyz_GteCommand(0x048a012)
#undef gte_rtv2bk
#define gte_rtv2bk() Psyz_GteCommand(0x0492012)
#undef gte_rtirbk
#define gte_rtirbk() Psyz_GteCommand(0x049a012)
#undef gte_ll
#define gte_ll() Psyz_GteCommand(0x04a6412)
#undef gte_llv0
#define gte_llv0() Psyz_GteCommand(0x04a6012)
#undef gte_llv1
#define gte_llv1() Psyz_GteCommand(0x04ae012)
#undef gte_llv2
#define gte_llv2() Psyz_GteCommand(0x04b6012)
#undef gte_llir
#define gte_llir() Psyz_GteCommand(0x04be012)
#undef gte_llv0tr
#define gte_llv0tr() Psyz_GteCommand(0x04a0012)
#undef gte_llv1tr
#define gte_llv1tr() Psyz_GteCommand(0x04a8012)
#undef gte_llv2tr
#define gte_llv2tr() Psyz_GteCommand(0x04b0012)
#undef gte_llirtr
#define gte_llirtr() Psyz_GteCommand(0x04b8012)
#undef gte_llv0bk
#define gte_llv0bk() Psyz_GteCommand(0x04a2012)
#undef gte_llv1bk
#define gte_llv1bk() Psyz_GteCommand(0x04aa012)
#undef gte_llv2bk
#define gte_llv2bk() Psyz_GteCommand(0x04b2012)
#undef gte_llirbk
#define gte_llirbk() Psyz_GteCommand(0x04ba012)
#undef gte_lc
#define gte_lc() Psyz_GteCommand(0x04da412)
#undef gte_lcv0
#define gte_lcv0() Psyz_GteCommand(0x04c6012)
#undef gte_lcv1
#define gte_lcv1() Psyz_GteCommand(0x04ce012)
#undef gte_lcv2
#define gte_lcv2() Psyz_GteCommand(0x04d6012)
#undef gte_lcir
#define gte_lcir() Psyz_GteCommand(0x04de012)
#undef gte_lcv0tr
#define gte_lcv0tr() Psyz_GteCommand(0x04c0012)
#undef gte_lcv1tr
#define gte_lcv1tr() Psyz_GteCommand(0x04c8012)
#undef gte_lcv2tr
#define gte_lcv2tr() Psyz_GteCommand(0x04d0012)
#undef gte_lcirtr
#define gte_lcirtr() Psyz_GteCommand(0x04d8012)
#undef gte_lcv0bk
#define gte_lcv0bk() Psyz_GteCommand(0x04c2012)
#undef gte_lcv1bk
#define gte_lcv1bk() Psyz_GteCommand(0x04ca012)
#undef gte_lcv2bk
#define gte_lcv2bk() Psyz_GteCommand(0x04d2012)
#undef gte_lcirbk
#define gte_lcirbk() Psyz_GteCommand(0x04da012)
#undef gte_dpcl
#define gte_dpcl() Psyz_GteCommand(0x0680029)
#undef gte_dpcs
#define gte_dpcs() Psyz_GteCommand(0x0780010)
#undef gte_dpct
#define gte_dpct() Psyz_GteCommand(0x0f8002a)
#undef gte_intpl
#define gte_intpl() Psyz_GteCommand(0x0980011)
#undef gte_ncs
#define gte_ncs() Psyz_GteCommand(0x0c8041e)
#undef gte_nct
#define gte_nct() Psyz_GteCommand(0x0d80420)
#undef gte_ncds
#define gte_ncds() Psyz_GteCommand(0x0e80413)
#undef gte_ncdt
#define gte_ncdt() Psyz_GteCommand(0x0f80416)
#undef gte_nccs
#define gte_nccs() Psyz_GteCommand(0x108041b)
#undef gte_ncct
#define gte_ncct() Psyz_GteCommand(0x118043f)
#undef gte_cdp
#define gte_cdp() Psyz_GteCommand(0x1280414)
#undef gte_cc
#define gte_cc() Psyz_GteCommand(0x138041c)
#undef gte_nclip
#define gte_nclip() Psyz_GteCommand(0x1400006)
#undef gte_avsz3
#define gte_avsz3() Psyz_GteCommand(0x158002d)
#undef gte_avsz4
#define gte_avsz4() Psyz_GteCommand(0x168002e)
#undef gte_sqr12
#define gte_sqr12() Psyz_GteCommand(0x0a80428)
#undef gte_sqr0
#define gte_sqr0() Psyz_GteCommand(0x0a00428)
#undef gte_op12
#define gte_op12() Psyz_GteCommand(0x178000c)
#undef gte_op0
#define gte_op0() Psyz_GteCommand(0x170000c)
#undef gte_gpf12
#define gte_gpf12() Psyz_GteCommand(0x198003d)
#undef gte_gpf0
#define gte_gpf0() Psyz_GteCommand(0x190003d)
#undef gte_gpl12
#define gte_gpl12() Psyz_GteCommand(0x1a8003e)
#undef gte_gpl0
#define gte_gpl0() Psyz_GteCommand(0x1a0003e)

#endif
