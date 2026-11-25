#if defined(LOM_NATIVE_GAME_CODE)
#include "sdk/fcntl.h"
#else
#include_next <fcntl.h>
#endif
