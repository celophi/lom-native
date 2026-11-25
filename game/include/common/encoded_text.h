#ifndef ENCODED_TEXT_H
#define ENCODED_TEXT_H

#include "common.h"

#define TEXT_TABLE_STRING(table, index) ((u8*)(table) + ((u16*)(table))[index])

#define ENCODED_TEXT_DOUBLE_BYTE_LEAD_FIRST 0x19

#define ENCODED_TEXT_DOUBLE_BYTE_LEAD_LAST 0x1F

#define ENCODED_TEXT_IS_DOUBLE_BYTE_LEAD(code) ((code) >= ENCODED_TEXT_DOUBLE_BYTE_LEAD_FIRST && (code) <= ENCODED_TEXT_DOUBLE_BYTE_LEAD_LAST)

void encoded_text_append(u8* dst, u8* src);
s32 encoded_text_byte_length(u8* text);
void encoded_text_copy(u8* dst, u8* src);

#endif
