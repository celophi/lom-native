#ifndef LOM_NATIVE_SDK_LIBGPU_H
#define LOM_NATIVE_SDK_LIBGPU_H

#include_next <libgpu.h>

#ifndef setTexWindow
#define setTexWindow(prim, window) SetTexWindow((DR_TWIN*)(prim), (RECT*)(window))
#endif

#endif
