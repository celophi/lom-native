#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>

/** True once a decoder has exited (Windows has no zombies to reap). */
int lom_test_process_exited(long long id)
{
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)id);

    if (process == NULL)
    {
        return 1;
    }

    int exited = WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
    CloseHandle(process);
    return exited;
}

/**
 * @brief A stream whose reads fail, standing in for a decoder pipe that breaks.
 *
 * Windows has no fopencookie() or funopen() to inject one failed read into a
 * working stream, so the failure is a file opened only for writing. Its reads
 * fail with EBADF rather than EIO, which the decoder handles the same way.
 * @return The stream, or NULL.
 */
FILE* lom_test_unreadable_stream(void)
{
    char path[MAX_PATH], folder[MAX_PATH];

    if (!GetTempPathA(sizeof(folder), folder) || !GetTempFileNameA(folder, "lom", 0, path))
    {
        return NULL;
    }

    int fd = _open(path, _O_WRONLY | _O_BINARY | _O_TEMPORARY);
    FILE* stream = fd >= 0 ? _fdopen(fd, "rb") : NULL;

    if (stream == NULL && fd >= 0)
    {
        _close(fd);
    }

    return stream;
}

/** This program's own path. */
int lom_test_executable_path(char* path, size_t size)
{
    DWORD length = GetModuleFileNameA(NULL, path, (DWORD)size);
    return length != 0 && length < size ? 0 : -1;
}

/** The temporary folder, ending in a separator. */
int lom_test_temporary_folder(char* path, size_t size)
{
    DWORD length = GetTempPathA((DWORD)size, path);
    return length != 0 && length < size ? 0 : -1;
}

/** Copy a file to a new name that mustn't exist yet. */
int lom_test_copy_file(const char* from, const char* to)
{
    return CopyFileA(from, to, TRUE) ? 0 : -1;
}
