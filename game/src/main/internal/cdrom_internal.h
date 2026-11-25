#ifndef CDROM_INTERNAL_H
#define CDROM_INTERNAL_H

#include "main/cdrom.h"
#include "main/cdrom_decompress.h"
#include <libetc.h>

#define CD_STREAM_BUFFER_START ((u8*)CD_STREAM_BUFFER_ADDRESS)
#define CD_STREAM_BUFFER_SECTORS 4
#define CD_STREAM_BUFFER_LIMIT (CD_STREAM_BUFFER_START + (CD_STREAM_BUFFER_SECTORS * CD_DATA_SECTOR_SIZE))
#define CD_STREAM_DECOMPRESS_GUARD_SIZE 280
#define CD_STREAM_WRAP_START (CD_STREAM_BUFFER_START + CD_STREAM_DECOMPRESS_GUARD_SIZE)
#define CD_DECOMPRESS_UNBOUNDED_END ((u8*)0xFFFFFFFCU)

typedef union
{
    u8* bytes;
    u32* words;
} CdStreamCopyCursor;

typedef struct
{
    volatile u8 data_ready;
    volatile u8 input_complete;
    u8 _pad02[2];
    u8* buffer_start;
    u8* input_cursor;
    s32 bytes_buffered;
    s32 wrap_overflow;
    s32 bytes_consumed;
    s32 deferred_sectors;
} CdStreamState;

#define CD_STREAM_STATE (*(CdStreamState*)getScratchAddr(0))

u8* cdrom_handle_stream_data(s32 byte_count, u32 bytes_remaining);
void cdrom_clear_data_ready(volatile u8* data_ready);

#endif
