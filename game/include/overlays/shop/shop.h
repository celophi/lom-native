#ifndef SHOP_H
#define SHOP_H

#include "common.h"
#include "main/main.h"
#include "main/display.h"
#include "main/field_runtime.h"

typedef struct
{
    u8 packets[FRAME_PRIMITIVE_BUFFER_SIZE];
    u8* prim_cursor;
} ShopPacketBuffer;

#define SHOP_ENTRY_RECORD_FLAG 0x8000
#define SHOP_ENTRY_RECORD_INDEX_MASK 0x7FFF

typedef struct ShopEntry
{
    u16 id;
    u16 count;
    s32 price;
} ShopEntry;

void shop_init(u8* work, s32 is_buying, s32 entry_count, ShopEntry* entries, FieldItemRecord* item_records, s32 title_text_id);
s32 shop_update(FieldRenderHalf* render);

#endif
