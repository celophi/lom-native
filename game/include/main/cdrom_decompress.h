#ifndef CDROM_DECOMPRESS_H
#define CDROM_DECOMPRESS_H

#include "common.h"

void cdrom_decompress_buffer(u8* source, u8* destination);

s32 cdrom_decompress_data(u8** src_cursor, u8** dst_cursor, u8* src_end, u8* dst_end);

#endif
