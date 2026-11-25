#ifndef LOM_NATIVE_PLATFORM_H
#define LOM_NATIVE_PLATFORM_H

#ifdef _WIN32
#define LOM_PLATFORM_WINDOWS 1
#else
#define LOM_PLATFORM_WINDOWS 0
#endif

#ifdef __APPLE__
#define LOM_PLATFORM_APPLE 1
#else
#define LOM_PLATFORM_APPLE 0
#endif

#endif
