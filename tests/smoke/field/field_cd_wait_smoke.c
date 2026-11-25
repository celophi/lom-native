#define u_long LomHostULong
#include "lom_native_setjmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef u_long
#include "main/cdrom.h"
#include "lom_native_cd.h"
#include "lom_native_field.h"
#include "lom_native_ps1_memory.h"
#include "../../support/lom_check.h"

static LomJumpBuffer rejected;

static int expecting_failure, ready;

static unsigned callbacks;

static char failure[128];

static u8 sector[2048], following[2017];

_Noreturn void lom_native_field_fail(const char* reason)
{
    CHECK(expecting_failure && reason);
    CHECK(snprintf(failure, sizeof(failure), "%s", reason) < (int)sizeof(failure));
    LOM_LONGJMP(rejected, 1);
}

/**
 * @brief A sector callback that takes the first sector, then holds the second
 *        back.
 * @param done Bytes delivered so far.
 * @param remaining Bytes still to come.
 * @return Where the sector goes, or NULL to hold it.
 */
static u8* consumer(s32 done, u32 remaining)
{
    CHECK((done == 0 && remaining == 2785) || (done == 2048 && remaining == 737));

    if (ready)
    {
        return NULL;
    }

    ready = 1;
    ++callbacks;
    return sector;
}

/**
 * @brief A sector callback that points at the last byte of RAM, where a sector
 *        doesn't fit.
 * @param done Ignored.
 * @param remaining Ignored.
 * @return The last byte of RAM.
 */
static u8* invalid_destination(s32 done, u32 remaining)
{
    (void)done;
    (void)remaining;
    return (void*)0x801FFFFF;
}

/**
 * @brief Check a CD wait stops FIELD.
 */
static void rejected_wait(void)
{
    expecting_failure = 1;

    if (!LOM_SETJMP(rejected))
    {
        cdrom_wait_queue_empty();
        CHECK(0);
    }

    expecting_failure = 0;
}

/**
 * @brief Check the read queued after the callback read got FONT.PRS whole.
 * @param root The disc's folder.
 */
static void check_following(const char* root)
{
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/MAP/FDATA/FONT.PRS", root) < (int)sizeof(path));
    FILE* file = fopen(path, "rb");
    CHECK(file);
    u8 expected[2013];
    CHECK(fread(expected, 1, sizeof(expected), file) == sizeof(expected) && !fclose(file));
    CHECK(!memcmp(following, expected, sizeof(expected)));

    for (unsigned i = 2013; i < sizeof(following); ++i)
    {
        CHECK(following[i] == 0xA5);
    }
}

int main(int argc, char** argv)
{
    CHECK(argc == 2 && !lom_native_map_ps1_ram() && !lom_native_cd_set_disc_root(argv[1]));
    lom_native_cd_set_hooks(&lom_native_field_cd_hooks);
    cdrom_init();
    cdrom_wait_queue_empty();
    memset(following, 0xA5, sizeof(following));
    CHECK(cdrom_queue_read_with_callback(0xB1, consumer) == 2785);
    CHECK(cdrom_queue_read(0xB2, following) == 2013);
    rejected_wait();
    CHECK(strstr(failure, "unconsumed callback") && callbacks == 1);
    CHECK(lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_CALLBACK_BLOCKED);

    for (unsigned i = 0; i < sizeof(following); ++i)
    {
        CHECK(following[i] == 0xA5);
    }

    CHECK(!cdrom_can_queue_resource(0xB1) && !cdrom_can_queue_resource(0xB2));
    ready = 0;
    cdrom_wait_queue_empty();
    CHECK(callbacks == 2 && !cdrom_get_error_status());
    check_following(argv[1]);
    cdrom_reset();
    CHECK(cdrom_queue_read_with_callback(0xB1, invalid_destination) == 2785);
    rejected_wait();
    CHECK(strstr(failure, "CD wait failed") && lom_native_cd_get_last_error() == LOM_NATIVE_CD_ERROR_INVALID_DESTINATION);
    cdrom_reset();
    memset(following, 0xA5, sizeof(following));
    CHECK(cdrom_queue_read(0xB2, following) == 2013);
    cdrom_wait_queue_empty();
    check_following(argv[1]);
    cdrom_stop();
    puts("FIELD CD waits: blocked FIFO loads fail before stale-buffer use, consumer release resumes exact bytes, transport errors fail, idle reads unchanged.");
    return 0;
}
