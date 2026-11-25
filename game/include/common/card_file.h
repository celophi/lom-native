#ifndef CARD_FILE_H
#define CARD_FILE_H

#include "common.h"

#define CARD_FILE_HEADER_BYTES 512

#define CARD_FILE_TITLE_OFFSET 4

#define CARD_FILE_TITLE_LINE_BYTES 0x20
#define CARD_FILE_TITLE_BYTES (CARD_FILE_TITLE_LINE_BYTES * 2)

#define CARD_FILE_RESERVED_BYTES 0x1C

#define CARD_FILE_ICON_CLUT_BYTES 0x20

#define CARD_FILE_ICON_MAX_FRAMES 3
#define CARD_FILE_ICON_FRAME_BYTES 0x80

#define CARD_FILE_ICON_TYPE(frames) (0x10 + (frames))

typedef struct
{
    char magic[2];
    u8 icon_type;
    u8 block_count;
    u8 title[2][CARD_FILE_TITLE_LINE_BYTES];
    u8 reserved[CARD_FILE_RESERVED_BYTES];
    u8 clut[CARD_FILE_ICON_CLUT_BYTES];
    u8 icon_frames[CARD_FILE_ICON_MAX_FRAMES][CARD_FILE_ICON_FRAME_BYTES];
} CardFileHeader;

#endif
