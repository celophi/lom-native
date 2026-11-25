#define WIN32_LEAN_AND_MEAN
#include "movie_process.h"

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define CHILD_COUNT 8

static struct
{
    pid_t id;

    HANDLE process;
} s_children[CHILD_COUNT];

/**
 * @brief Take a started ffmpeg's process handle by its id, on Windows.
 * @param id The id the caller has.
 * @return The handle, or NULL.
 */
static HANDLE take_child(pid_t id)
{
    for (int i = 0; i < CHILD_COUNT; ++i)
    {
        if (s_children[i].process != NULL && s_children[i].id == id)
        {
            HANDLE process = s_children[i].process;
            s_children[i].process = NULL;
            return process;
        }
    }

    return NULL;
}

/**
 * @brief Build a Windows command line from arguments, quoted the way
 *        CreateProcess's C runtime splits them.
 * @param args The arguments, NULL-ended.
 * @return The command line, which the caller frees, or NULL.
 */
static char* command_line(char* const args[])
{
    size_t size = 1;

    for (int i = 0; args[i] != NULL; ++i)
    {
        size += 2 * strlen(args[i]) + 3;
    }

    char* line = malloc(size);
    char* out = line;

    if (line == NULL)
    {
        return NULL;
    }

    for (int i = 0; args[i] != NULL; ++i)
    {
        const char* arg = args[i];

        if (i != 0)
        {
            *out++ = ' ';
        }

        if (*arg != 0 && strpbrk(arg, " \t\"") == NULL)
        {
            strcpy(out, arg);
            out += strlen(arg);
            continue;
        }

        *out++ = '"';

        for (const char* p = arg;; ++p)
        {
            size_t slashes = 0;

            while (*p == '\\')
            {
                ++slashes;
                ++p;
            }

            size_t copies = *p == 0 ? slashes * 2 : *p == '"' ? slashes * 2 + 1 : slashes;

            for (size_t k = 0; k < copies; ++k)
            {
                *out++ = '\\';
            }

            if (*p == 0)
            {
                break;
            }

            *out++ = *p;
        }

        *out++ = '"';
    }

    *out = 0;
    return line;
}

/**
 * @brief Find a free slot to keep a started ffmpeg's process handle in.
 * @return The slot, or CHILD_COUNT when every one is in use.
 */
static int free_child_slot(void)
{
    int slot;

    for (slot = 0; slot < CHILD_COUNT && s_children[slot].process != NULL; ++slot)
    {
    }

    return slot;
}

/**
 * @brief Turn a UTF-8 command line into the UTF-16 one CreateProcessW takes.
 * @param line The command line.
 * @return It in UTF-16, for the caller to free, or NULL.
 */
static wchar_t* wide_command_line(const char* line)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, line, -1, NULL, 0);
    wchar_t* wide = length > 0 ? malloc((size_t)length * sizeof(wchar_t)) : NULL;

    if (wide == NULL || MultiByteToWideChar(CP_UTF8, 0, line, -1, wide, length) == 0)
    {
        free(wide);
        return NULL;
    }

    return wide;
}

/**
 * @brief Start ffmpeg with its output going to a pipe, its input from NUL and
 *        its errors to the game's stderr, passing it only those handles.
 * @param line Its command line.
 * @param write_end The pipe's write end, for its output.
 * @param process Set to its process and thread.
 * @return 0, or the Windows error when it couldn't start.
 */
static DWORD create_decoder_process(wchar_t* line, HANDLE write_end, PROCESS_INFORMATION* process)
{
    SECURITY_ATTRIBUTES inheritable = {sizeof(inheritable), NULL, TRUE};
    HANDLE null_device, error_output = NULL;
    HANDLE inherited[3];
    DWORD inherited_count = 0, error = 0;
    SIZE_T attribute_size = 0;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes = NULL;
    STARTUPINFOEXW startup;

    SetHandleInformation(write_end, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    null_device = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, 0, NULL);
    HANDLE parent_error = GetStdHandle(STD_ERROR_HANDLE);

    if (parent_error == NULL || parent_error == INVALID_HANDLE_VALUE ||
        !DuplicateHandle(GetCurrentProcess(), parent_error, GetCurrentProcess(), &error_output, 0, TRUE, DUPLICATE_SAME_ACCESS))
    {
        error_output = NULL;
    }

    memset(&startup, 0, sizeof(startup));
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput = write_end;
    startup.StartupInfo.hStdInput = null_device;
    startup.StartupInfo.hStdError = error_output != NULL ? error_output : null_device;
    inherited[inherited_count++] = write_end;

    if (null_device != INVALID_HANDLE_VALUE)
    {
        inherited[inherited_count++] = null_device;
    }

    if (error_output != NULL)
    {
        inherited[inherited_count++] = error_output;
    }

    InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_size);
    attributes = malloc(attribute_size);

    if (attributes == NULL || !InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size))
    {
        free(attributes);
        attributes = NULL;
        error = ERROR_NOT_ENOUGH_MEMORY;
    }
    else if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, inherited_count * sizeof(HANDLE), NULL, NULL))
    {
        error = GetLastError();
    }

    if (error == 0)
    {
        startup.lpAttributeList = attributes;

        if (!CreateProcessW(NULL, line, NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, NULL, NULL, &startup.StartupInfo, process))
        {
            error = GetLastError();
        }
    }

    if (attributes != NULL)
    {
        DeleteProcThreadAttributeList(attributes);
        free(attributes);
    }

    if (null_device != INVALID_HANDLE_VALUE)
    {
        CloseHandle(null_device);
    }

    if (error_output != NULL)
    {
        CloseHandle(error_output);
    }

    return error;
}

/**
 * @brief Keep a started ffmpeg's process handle in a slot, and open its output
 *        as a stream. When the stream can't be opened, ffmpeg is stopped.
 * @param slot A free slot.
 * @param process Its process and thread.
 * @param read_end The read end of its output.
 * @param child Set to its id.
 * @return The stream, or NULL.
 */
static FILE* adopt_child(int slot, const PROCESS_INFORMATION* process, HANDLE read_end, pid_t* child)
{
    CloseHandle(process->hThread);
    s_children[slot].id = (pid_t)process->dwProcessId;
    s_children[slot].process = process->hProcess;
    *child = s_children[slot].id;

    int fd = _open_osfhandle((intptr_t)read_end, _O_RDONLY | _O_BINARY);
    FILE* stream = fd >= 0 ? _fdopen(fd, "rb") : NULL;

    if (stream == NULL)
    {
        int saved_errno = errno;
        HANDLE handle = take_child(*child);
        TerminateProcess(handle, 1);
        WaitForSingleObject(handle, INFINITE);
        CloseHandle(handle);

        if (fd >= 0)
        {
            _close(fd);
        }
        else
        {
            CloseHandle(read_end);
        }

        errno = saved_errno;
    }

    return stream;
}

/**
 * @brief Start ffmpeg on Windows with its output piped back and its errors sent
 *        to the game's stderr.
 * @param args Its arguments, NULL-ended.
 * @param child Set to its id.
 * @return The read end of its output, or NULL.
 */
FILE* lom_movie_process_start(char* const args[], pid_t* child)
{
    char* line = command_line(args);
    const int slot = free_child_slot();
    HANDLE read_end = NULL, write_end = NULL;
    PROCESS_INFORMATION process;

    if (line == NULL || slot == CHILD_COUNT)
    {
        free(line);
        errno = line == NULL ? ENOMEM : EMFILE;
        return NULL;
    }

    wchar_t* wide_line = wide_command_line(line);
    free(line);

    if (wide_line == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (!CreatePipe(&read_end, &write_end, NULL, 1 << 16))
    {
        free(wide_line);
        errno = EMFILE;
        return NULL;
    }

    const DWORD error = create_decoder_process(wide_line, write_end, &process);
    free(wide_line);
    CloseHandle(write_end);

    if (error != 0)
    {
        CloseHandle(read_end);
        errno = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ENOENT : EIO;
        fprintf(stderr, "[lom-native] Could not spawn ffmpeg: %s (Windows error %lu)\n", strerror(errno), (unsigned long)error);
        return NULL;
    }

    return adopt_child(slot, &process, read_end, child);
}

void lom_movie_process_cancel(pid_t child)
{
    for (int i = 0; i < CHILD_COUNT; ++i)
    {
        if (s_children[i].process != NULL && s_children[i].id == child)
        {
            TerminateProcess(s_children[i].process, 1);
        }
    }
}

void lom_movie_process_wait(pid_t child, LomMovieCloseResult* result)
{
    HANDLE process = take_child(child);
    DWORD code = 0;

    if (process == NULL || WaitForSingleObject(process, INFINITE) != WAIT_OBJECT_0 || !GetExitCodeProcess(process, &code))
    {
        result->wait_error = ECHILD;
    }
    else
    {
        result->wait_status = (int)code;
    }

    if (process != NULL)
    {
        CloseHandle(process);
    }
}

int lom_movie_process_exited_cleanly(int wait_status)
{
    return wait_status == 0;
}
