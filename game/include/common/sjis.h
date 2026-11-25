#ifndef SJIS_H
#define SJIS_H

#include "common.h"

#define SJIS_LEAD_MIN 0x80

#define SJIS_MULTIBYTE_CHAR_BYTES 2

#define SJIS_DIGIT_ZERO 0x824F

#define SJIS_NOTE_SIGN 0x81F4

#define SJIS_CODE_LEAD_SHIFT 8
#define SJIS_CODE_TRAIL_MASK 0xFF

#define SJIS_DECIMAL_RADIX 10
#define SJIS_DECIMAL_FIRST_DIVISOR 100000

#define SJIS_DECIMAL_OVERFLOW_TEXT "\x82\x6c\x82\x60\x82\x77"

/**
 * @brief Format @p value as full-width Shift-JIS digits without leading zeros and null-terminate it.
 * @param destination Buffer with room for up to six two-byte digits and a null byte.
 * @param value Nonnegative value to format; 1000000 and up becomes full-width "MAX".
 * @return Pointer to the terminator.
 */
s8* format_decimal(s8* destination, s32 value);

#endif
