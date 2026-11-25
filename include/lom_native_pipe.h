#ifndef LOM_NATIVE_PIPE_H
#define LOM_NATIVE_PIPE_H

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#endif

/**
 * @brief Open a pipe whose ends close when the process runs another program.
 *
 * Linux does it in one call with pipe2(). macOS has no pipe2(), so there the
 * flag goes on each end right after. Nothing the port runs starts a program
 * in between. On Windows, _pipe() makes both ends uninheritable in one call,
 * and in binary mode so nothing turns \n into \r\n.
 * @param fds Receives the read end, then the write end.
 * @return 0 on success, or -1 with errno set and nothing left open.
 */
static inline int lom_native_pipe_cloexec(int fds[2])
{
#if defined(_WIN32)
    return _pipe(fds, 65536, _O_BINARY | _O_NOINHERIT);
#elif defined(__APPLE__)
    if (pipe(fds) != 0)
    {
        return -1;
    }

    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(fds[1], F_SETFD, FD_CLOEXEC) != 0)
    {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    return 0;
#else
    return pipe2(fds, O_CLOEXEC);
#endif
}

/**
 * @brief Make a pipe's read end return at once when nothing is waiting, for
 *        lom_native_pipe_read. Windows needs nothing: lom_native_pipe_read asks
 *        the pipe first instead.
 * @param fd The read end.
 */
static inline void lom_native_pipe_set_nonblocking(int fd)
{
#ifdef _WIN32
    (void)fd;
#else
    fcntl(fd, F_SETFL, O_NONBLOCK);
#endif
}

/**
 * @brief Read what's waiting in a pipe without blocking.
 * @param fd The read end, set up with lom_native_pipe_set_nonblocking.
 * @param buffer Where the bytes go.
 * @param size Room at @p buffer.
 * @return How many bytes it read; 0 when the pipe is closed; or -1 with errno
 *         EAGAIN when nothing is waiting, or another errno when the read fails.
 */
static inline ssize_t lom_native_pipe_read(int fd, void* buffer, size_t size)
{
#ifdef _WIN32
    DWORD waiting = 0;

    if (!PeekNamedPipe((HANDLE)_get_osfhandle(fd), NULL, 0, NULL, &waiting, NULL))
    {
        return 0;
    }

    if (waiting == 0)
    {
        errno = EAGAIN;
        return -1;
    }
#endif
    return read(fd, buffer, size);
}

#endif
