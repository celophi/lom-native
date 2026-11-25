#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "card_slot.h"
#include "lom_native_fs.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int card_slot_open(const char* folder, int slot)
{
    (void)slot;
    return open(folder, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
}

void card_slot_close(int dir)
{
    close(dir);
}

DIR* card_slot_list(int dir)
{
    return fdopendir(dir);
}

int card_slot_open_file(int dir, const char* name, int flags)
{
    return openat(dir, name, flags | LOM_NATIVE_O_PRIVATE | O_NOFOLLOW, 0600);
}

int card_slot_stat(int dir, const char* name, struct stat* st)
{
    return fstatat(dir, name, st, AT_SYMLINK_NOFOLLOW);
}

int card_slot_unlink(int dir, const char* name)
{
    return unlinkat(dir, name, 0);
}

int card_slot_rename(int dir, const char* from, const char* to)
{
#ifdef __APPLE__
    return renameatx_np(dir, from, dir, to, RENAME_EXCL);
#else
    return renameat2(dir, from, dir, to, RENAME_NOREPLACE);
#endif
}

int card_slot_sync(int dir)
{
    return fsync(dir);
}

int card_file_sync(int fd)
{
    return fsync(fd);
}

ssize_t card_file_transfer(int fd, void* data, size_t size, off_t offset, int writing)
{
    return writing ? pwrite(fd, data, size, offset) : pread(fd, data, size, offset);
}
