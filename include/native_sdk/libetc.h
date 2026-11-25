#ifndef LOM_NATIVE_SDK_LIBETC_H
#define LOM_NATIVE_SDK_LIBETC_H

#include_next <libetc.h>
#include "common/memory_map.h"

#undef getScratchAddr
#define getScratchAddr(offset) ((u_long*)SCRATCHPAD_AT((offset) * 4))

#endif
