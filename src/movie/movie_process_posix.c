#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "movie_process.h"
#include "lom_native_pipe.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

extern char** environ;

/**
 * @brief Find the ffmpeg that comes with the game, next to it.
 * @param path Set to its path.
 * @param size Bytes at @p path.
 * @return 1 when there is one, otherwise 0 to use the one on the PATH.
 */
static int find_bundled_decoder(char* path, size_t size)
{
    char self[PATH_MAX];
    char* slash;
#ifdef __APPLE__
    char link[PATH_MAX];
    uint32_t capacity = sizeof(link);

    if (_NSGetExecutablePath(link, &capacity) != 0 || realpath(link, self) == NULL)
    {
        return 0;
    }
#else
    ssize_t length = readlink("/proc/self/exe", self, sizeof(self) - 1);

    if (length <= 0)
    {
        return 0;
    }

    self[length] = '\0';
#endif
    slash = strrchr(self, '/');

    if (slash == NULL)
    {
        return 0;
    }

    slash[1] = '\0';

    if ((size_t)snprintf(path, size, "%sffmpeg", self) >= size)
    {
        return 0;
    }

    return access(path, X_OK) == 0;
}

FILE* lom_movie_process_start(char* const args[], pid_t* child)
{
    posix_spawn_file_actions_t actions;
    int fds[2];
    int error;
    FILE* stream;
    char bundled[PATH_MAX];

    if (lom_native_pipe_cloexec(fds) != 0)
    {
        return NULL;
    }

    error = posix_spawn_file_actions_init(&actions);

    if (error == 0)
    {
        error = posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);

        if (error == 0)
        {
            error = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        }

        if (error == 0)
        {
            if (find_bundled_decoder(bundled, sizeof(bundled)))
            {
                error = posix_spawn(child, bundled, &actions, NULL, args, environ);
            }
            else
            {
                error = posix_spawnp(child, "ffmpeg", &actions, NULL, args, environ);
            }
        }

        posix_spawn_file_actions_destroy(&actions);
    }

    close(fds[1]);

    if (error != 0)
    {
        close(fds[0]);
        fprintf(stderr, "[lom-native] Could not spawn ffmpeg: %s\n", strerror(error));
        errno = error;
        return NULL;
    }

    stream = fdopen(fds[0], "r");

    if (stream == NULL)
    {
        int saved_errno = errno;
        kill(*child, SIGTERM);
        close(fds[0]);

        while (waitpid(*child, NULL, 0) < 0 && errno == EINTR)
        {
        }
        errno = saved_errno;
    }

    return stream;
}

void lom_movie_process_cancel(pid_t child)
{
    kill(child, SIGTERM);
}

void lom_movie_process_wait(pid_t child, LomMovieCloseResult* result)
{
    pid_t waited;

    do
    {
        waited = waitpid(child, &result->wait_status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited != child)
    {
        result->wait_error = errno != 0 ? errno : ECHILD;
    }
}

int lom_movie_process_exited_cleanly(int wait_status)
{
    return WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0;
}
