#ifndef VERSION_H
#define VERSION_H

#if !defined(VERSION_US) && !defined(VERSION_JP)
#define VERSION_US
#endif

#if defined(VERSION_US) && defined(VERSION_JP)
#error "Define only one of VERSION_US and VERSION_JP"
#endif

#ifndef ASM_VERSION_DIR
#if defined(VERSION_JP)
#define ASM_VERSION_DIR "asm/jp"
#else
#define ASM_VERSION_DIR "asm/us"
#endif
#endif

#endif
