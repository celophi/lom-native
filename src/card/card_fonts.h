#ifndef LOM_NATIVE_CARD_FONTS_H
#define LOM_NATIVE_CARD_FONTS_H
#include <stddef.h>
#include <stdint.h>

extern const uint8_t _binary_charset_font1_raw_start[];

extern const uint8_t _binary_charset_font2_raw_start[];

extern const size_t lom_native_font1_bytes;

extern const size_t lom_native_font2_bytes;

/**
 * @brief Find a Shift-JIS character in OpenBIOS's fonts (sjis.h).
 * @param c The character code.
 * @return Its glyph's index in the fonts.
 */
uint16_t Krom2Offset(uint32_t c);

#endif
