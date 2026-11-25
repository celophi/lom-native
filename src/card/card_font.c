#include "card_fonts.h"

#ifdef _WIN64
typedef long long KromAddress;
#else
typedef long KromAddress;
#endif

KromAddress lom_native_card_krom(unsigned int code)
{
    unsigned c = (unsigned)code & 0xffff;
    const uint8_t* base;
    size_t size;

    if (c >= 0x8140 && c <= 0x84be)
    {
        base = _binary_charset_font1_raw_start;
        size = lom_native_font1_bytes;
    }
    else if (c >= 0x889f && c <= 0x9872)
    {
        base = _binary_charset_font2_raw_start;
        size = lom_native_font2_bytes;
    }
    else
    {
        return -1;
    }

    size_t offset = (size_t)Krom2Offset(c) * 30;

    if (offset > size || size - offset < 30)
    {
        return -1;
    }

    return (KromAddress)(uintptr_t)(base + offset);
}
