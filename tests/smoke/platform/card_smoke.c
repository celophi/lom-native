#include "lom_native_card.h"
#include "lom_native_fs.h"
#include "../../support/platform/lom_test_posix.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#endif
#include <unistd.h>
#include "../../support/lom_check.h"
static unsigned char expected[16384], actual[16384];

/**
 * @brief Check the save on the card reads back as written.
 * @return 0. A failed check stops the test.
 */
static int verify_file(void)
{
    int h = lom_native_card_open("bu00:SAVE", 0x8001);
    CHECK(h > 0);
    CHECK(lom_native_card_read_libapi(h, actual, sizeof(actual)) == (long)sizeof(actual));
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    CHECK(!lom_native_card_close(h));
    return 0;
}

int main(int argc, char** argv)
{
    for (unsigned i = 0; i < sizeof(expected); ++i)
    {
        expected[i] = (unsigned char)(i * 37u + (i >> 8));
    }

    if (argc == 3 && !strcmp(argv[1], "--verify"))
    {
        CHECK(!lom_native_card_set_root(argv[2]));
        return verify_file();
    }

    char root[4096], path[4096 + 64];
    CHECK(lom_test_make_temp_folder(root, sizeof(root), "lom-card-test"));
    CHECK(!lom_native_card_set_root(root));
    snprintf(path, sizeof(path), "%s/slot0", root);
    CHECK(!lom_native_make_folder(path));
    long complete = lom_native_card_open_event(0xF4000001u, 4, 0x2000, 0);
    long error = lom_native_card_open_event(0xF4000001u, 0x8000, 0x2000, 0);
    long timeout = lom_native_card_open_event(0xF4000001u, 0x100, 0x2000, 0);
    CHECK(complete > 0 && error > 0 && timeout > 0);
    CHECK(lom_native_card_enable_event(complete));
    CHECK(lom_native_card_enable_event(error));
    CHECK(lom_native_card_enable_event(timeout));
    CHECK(lom_native_card_info(0));
    CHECK(lom_native_card_test_event(complete) == 1);
    CHECK(!lom_native_card_test_event(complete));
    CHECK(lom_native_card_info(7));
    CHECK(lom_native_card_test_event(error));
    CHECK(!lom_native_card_test_event(complete));
    CHECK(lom_native_card_info(16));
    CHECK(lom_native_card_test_event(timeout) == 1);
    CHECK(!lom_native_card_test_event(complete));
    CHECK(lom_native_card_open_libapi("bu00:MISSING", 0x8001) == -1L);
    CHECK(lom_native_card_close_libapi(-1L) == -1L);
    CHECK(lom_native_card_read_libapi(-1L, actual, sizeof(actual)) == -1L);
    CHECK(lom_native_card_write_libapi(-1L, expected, sizeof(expected)) == -1L);
    CHECK(lom_native_card_erase_libapi("bu00:MISSING") == 0L);
    CHECK(lom_native_card_rename_libapi("bu00:MISSING", "bu00:OTHER") == 0L);
    LomNativeCardEntry e;
    CHECK(!lom_native_card_first("bu00:*", &e));
    CHECK(lom_native_card_open("bu00:../escape", 0x20200) < 0);
    CHECK(lom_native_card_open("bu00:dir/file", 0x20200) < 0);
    int h = lom_native_card_open("bu00:TEMP", 0x20200);
    CHECK(h > 0);
    CHECK(lom_native_card_set_root(root) < 0);
    CHECK(!lom_native_card_close(h));
    CHECK(lom_native_card_open("bu00:TEMP", 0x20200) < 0);
    h = lom_native_card_open("bu00:TEMP", 0x8002);
    CHECK(h > 0);
    CHECK(lom_native_card_write_libapi(h, expected, sizeof(expected)) == (long)sizeof(expected));
    CHECK(lom_native_card_test_event(complete));
    CHECK(lom_native_card_write(h, expected, 1) == -1);
    CHECK(lom_native_card_test_event(error));
    CHECK(!lom_native_card_close(h));
    CHECK(lom_native_card_rename("bu00:TEMP", "bu00:SAVE"));
    CHECK(!verify_file());
    CHECK(lom_native_card_first("bu00:*", &e) == &e);
    CHECK(!memcmp(e.name, "SAVE", 5) && e.size == 16384 && !e.next);
    CHECK(!lom_native_card_next(&e));
    CHECK(lom_native_card_first("bu00:SAVE", &e) == &e && e.head == 64);
    CHECK(!lom_native_card_next(&e) && !lom_native_card_first("bu00:MISSING", &e));
    long hardware = lom_native_card_open_event(0xF0000011u, 4, 0x2000, 0);
    long hardware_error = lom_native_card_open_event(0xF0000011u, 0x8000, 0x2000, 0);
    CHECK(hardware > 0 && hardware_error > 0);
    CHECK(lom_native_card_enable_event(hardware) && lom_native_card_enable_event(hardware_error));
    CHECK(lom_native_card_read_sector(0, 64, actual) && lom_native_card_test_event(hardware));
    CHECK(!memcmp(actual, expected, 128));
    actual[0] ^= 0x55;
    CHECK(lom_native_card_write_sector(0, 64, actual) && lom_native_card_test_event(hardware));
    expected[0] ^= 0x55;
    CHECK(!verify_file());
    expected[0] ^= 0x55;
    CHECK(lom_native_card_write_sector(0, 64, expected) && lom_native_card_test_event(hardware));
    CHECK(!verify_file());
    CHECK(!lom_native_card_read_sector(0, 63, actual) && lom_native_card_test_event(hardware_error));
    CHECK(!lom_native_card_write_sector(16, 64, actual) && lom_native_card_test_event(hardware_error));
    CHECK(!lom_native_card_format(0) && lom_native_card_test_event(error));
    CHECK(!verify_file());
    CHECK(lom_native_card_close_event(hardware) && lom_native_card_close_event(hardware_error));
    h = lom_native_card_open("bu00:SAVE", 0x8001);
    CHECK(h > 0);
    CHECK(lom_native_card_write(h, expected, 4) == -1);
    CHECK(lom_native_card_test_event(error));
    CHECK(!lom_native_card_close(h));
    CHECK(!verify_file());
#ifdef _WIN32
    CHECK(_spawnl(_P_WAIT, argv[0], argv[0], "--verify", root, (char*)NULL) == 0);
#else
    pid_t child = fork();
    CHECK(child >= 0);

    if (!child)
    {
        execl(argv[0], argv[0], "--verify", root, (char*)NULL);
        _exit(2);
    }

    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
    h = lom_native_card_open("bu00:FILL", (13 << 16) | 0x200);
    CHECK(h > 0);
    CHECK(!lom_native_card_close(h));
    CHECK(lom_native_card_open("bu00:FULL", 0x10200) < 0);
    CHECK(!verify_file());
    CHECK(!lom_native_card_rename("bu00:SAVE", "bu00:FILL"));
    CHECK(!verify_file());
    CHECK(lom_native_card_erase("bu00:FILL"));
    h = lom_native_card_open("bu00:12345678901234567890", 0x10200);
    CHECK(h > 0);
    CHECK(!lom_native_card_close(h));
    CHECK(lom_native_card_first("bu00:*", &e));
    CHECK(!memcmp(e.name, "12345678901234567890", 20));
    CHECK(lom_native_card_erase("bu00:12345678901234567890"));
    snprintf(path, sizeof(path), "%s/slot0/BROKEN", root);
    FILE* broken = fopen(path, "wb");
    CHECK(broken);
    CHECK(fputc(1, broken) != EOF);
    CHECK(!fclose(broken));
    CHECK(!lom_native_card_first("bu00:*", &e));
    CHECK(lom_native_card_test_event(error));
    CHECK(lom_native_card_open("bu00:BROKEN", 0x8001) < 0);
    CHECK(!unlink(path));
#ifndef _WIN32
    snprintf(path, sizeof(path), "%s/slot0/LINK", root);
    CHECK(!symlink("SAVE", path));
    CHECK(lom_native_card_open("bu00:LINK", 0x8002) < 0);
    CHECK(!unlink(path));
#endif
    CHECK(!verify_file());
    h = lom_native_card_open("bu00:SAVE", 0x8002);
    CHECK(h > 0);
    CHECK(lom_native_card_rename("bu00:SAVE", "bu00:MOVED"));
    CHECK(!lom_native_card_close(h));
    CHECK(lom_native_card_rename("bu00:MOVED", "bu00:SAVE"));
    CHECK(!verify_file());
    CHECK(!lom_native_card_rename("bu00:SAVE", "bu10:SAVE"));
    CHECK(lom_native_card_erase("bu00:SAVE"));
    CHECK(!lom_native_card_erase("bu00:SAVE"));
    CHECK(lom_native_card_close_event(complete));
    CHECK(!lom_native_card_test_event(complete));
    CHECK(lom_native_card_close_event(error));
    CHECK(lom_native_card_close_event(timeout));
    snprintf(path, sizeof(path), "%s/slot0", root);
    CHECK(!rmdir(path));
    CHECK(!rmdir(root));
    puts("Card: exact bytes survive a new process; card events, no-card, directory ABI, capacity, read-only and path containment pass");
    return 0;
}
