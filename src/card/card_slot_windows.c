#define WIN32_LEAN_AND_MEAN
#include "card_slot.h"
#include "lom_native_card.h"
#include "lom_native_fs.h"

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <windows.h>

/**
 * @brief Build a card file's full path.
 * @param dir The slot, as card_slot_open gave it.
 * @param name The file's name.
 * @param path Where to write the path.
 * @param size Bytes at @p path.
 * @return 0, or -1 with errno ENAMETOOLONG.
 */
static int slot_file_path(int dir, const char* name, char* path, size_t size)
{
    int n = snprintf(path, size, "%s/slot%d/%s", lom_native_card_root(), dir, name);

    if (n < 0 || n >= (int)size)
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

int card_slot_open(const char* folder, int slot)
{
    struct stat st;

    if (stat(folder, &st))
    {
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
        return -1;
    }

    return slot;
}

void card_slot_close(int dir)
{
    (void)dir;
}

DIR* card_slot_list(int dir)
{
    char folder[PATH_MAX];
    snprintf(folder, sizeof(folder), "%s/slot%d", lom_native_card_root(), dir);
    return opendir(folder);
}

int card_slot_open_file(int dir, const char* name, int flags)
{
    char path[PATH_MAX];

    if (slot_file_path(dir, name, path, sizeof(path)))
    {
        return -1;
    }

    int writable = (flags & O_ACCMODE) != O_RDONLY;
    DWORD creation = !(flags & O_CREAT) ? OPEN_EXISTING : (flags & O_EXCL) ? CREATE_NEW : OPEN_ALWAYS;
    HANDLE file = CreateFileA(path, GENERIC_READ | (writable ? GENERIC_WRITE : 0), FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, creation,
                              FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        errno = error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS      ? EEXIST
                : error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ENOENT
                                                                                 : EACCES;
        return -1;
    }

    int fd = _open_osfhandle((intptr_t)file, (writable ? 0 : _O_RDONLY) | _O_BINARY);

    if (fd < 0)
    {
        CloseHandle(file);
    }

    return fd;
}

int card_slot_stat(int dir, const char* name, struct stat* st)
{
    char path[PATH_MAX];
    return slot_file_path(dir, name, path, sizeof(path)) ? -1 : stat(path, st);
}

int card_slot_unlink(int dir, const char* name)
{
    char path[PATH_MAX];
    return slot_file_path(dir, name, path, sizeof(path)) ? -1 : unlink(path);
}

int card_slot_rename(int dir, const char* from, const char* to)
{
    char source[PATH_MAX], target[PATH_MAX];

    if (slot_file_path(dir, from, source, sizeof(source)) || slot_file_path(dir, to, target, sizeof(target)))
    {
        return -1;
    }

    return MoveFileExA(source, target, MOVEFILE_WRITE_THROUGH) ? 0 : -1;
}

int card_slot_sync(int dir)
{
    (void)dir;
    return 0;
}

int card_file_sync(int fd)
{
    return _commit(fd);
}

ssize_t card_file_transfer(int fd, void* data, size_t size, off_t offset, int writing)
{
    if (lseek(fd, offset, SEEK_SET) != offset)
    {
        return -1;
    }

    return writing ? write(fd, data, (unsigned)size) : read(fd, data, (unsigned)size);
}
