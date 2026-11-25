#include "lom_native_cd.h"
#include "lom_native_field_map.h"

#include "common/memory_map.h"
#include "main/cd_resources.h"
#include "main/cdrom.h"

#include <stdio.h>

#define MAP_COUNT 487
#define QUEUED_READ_LIMIT 15

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("usage: %s <extracted-disc-root>\n", argv[0]);
        return 2;
    }
    if (lom_native_cd_set_disc_root(argv[1]) != 0)
    {
        printf("Invalid extracted-disc root: %s\n", argv[1]);
        return 3;
    }
    cdrom_init();

    unsigned char* buffer = (unsigned char*)LOAD_BUFFER_ADDRESS;
    unsigned decoded = 0;
    unsigned records = 0;
    unsigned long largest = 0;
    for (int map = 0; map < MAP_COUNT; map++)
    {
        s32 resource = map + CD_RES_FIELD_MAP_BASE;
        if (cdrom_get_resource_size(resource) <= 0)
        {
            continue;
        }
        if (map < QUEUED_READ_LIMIT)
        {
            cdrom_queue_read(resource, buffer);
            cdrom_wait_queue_empty();
        }
        else
        {
            cdrom_stream(resource, buffer);
        }
        lom_native_field_decode_map(buffer);
        printf("map %3d: %4u records, %6lu bytes\n", map, lom_native_field_map_record_count(), lom_native_field_map_arena_bytes());
        decoded++;
        records += lom_native_field_map_record_count();
        if (lom_native_field_map_arena_bytes() > largest)
        {
            largest = lom_native_field_map_arena_bytes();
        }
    }
    printf("%u maps decoded, %u records, largest %lu bytes\n", decoded, records, largest);
    return decoded == 0;
}
