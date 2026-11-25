#ifndef SAVE_FILE_H
#define SAVE_FILE_H

#include "common.h"
#include "common/saved_game.h"
#include "common/sjis.h"

#define SAVE_HEX_MAX_DIGITS 8
#define SAVE_HEX_DIGIT_BITS 4
#define SAVE_HEX_DIGIT_MASK 0xF

#define SAVE_HEX_RADIX (SAVE_HEX_DIGIT_MASK + 1)
#define SAVE_HEX_DECIMAL_DIGITS 10

#define SAVE_HEX_INVALID_DIGIT '_'

#define SAVE_HEX_SUFFIX_DIGITS 2

s32 validate_save_file(SaveFile* file);
s32 compute_save_checksum(const void* save_data);
void terminate_multibyte_text(void* title_text);
u8* skip_hex_digits(u8* cursor);
void format_hex(char* destination, s32 value, s32 max_digits);
void hex_nibble_to_ascii(char* destination, s32 nibble);
u32 parse_hex(const u8* cursor, s32 digits_left);
s32 parse_hex_suffix_byte(const char* field_text);

#endif
