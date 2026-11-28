#define _GNU_SOURCE
#include "movie_decoder.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _WIN32
#include "../support/platform/lom_test_posix.h"
#include <fcntl.h>
#include <io.h>
int lom_test_process_exited(long long id);
int lom_test_executable_path(char* path, size_t size);
int lom_test_temporary_folder(char* path, size_t size);
int lom_test_copy_file(const char* from, const char* to);
#else
#include <sys/stat.h>
#include <sys/wait.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "../support/lom_check.h"

#define FRAME_BYTES (320u * 240u * 3u)

static unsigned char frame[FRAME_BYTES];

/**
 * @brief The test pattern's byte at an offset in a frame.
 * @param offset The offset.
 * @return The byte.
 */
static unsigned char pixel_byte(size_t offset)
{
    return (unsigned char)((offset * 17u + offset / 251u) & 255u);
}

/**
 * @brief Check the frame read holds the test pattern.
 * @param bytes How many bytes to check.
 * @return 1 when it does.
 */
static int check_frame(size_t bytes)
{
    for (size_t i = 0; i < bytes; ++i)
    {
        if (frame[i] != pixel_byte(i))
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Write all of a buffer, through interruptions.
 * @param fd Where to.
 * @param buffer The bytes.
 * @param size How many.
 * @return 0, or -1.
 */
static int write_bytes(int fd, const void* buffer, size_t size)
{
    size_t offset = 0;

    while (offset < size)
    {
        ssize_t written = write(fd, (const unsigned char*)buffer + offset, size - offset);

        if (written < 0 && errno == EINTR)
        {
            continue;
        }

        if (written <= 0)
        {
            return -1;
        }

        offset += (size_t)written;
    }

    return 0;
}

static const char tricky_input[] = "a name with \"quotes\" and \\ back\\slashes\\";

/**
 * @brief Start a process that writes the test pattern to a pipe and exits.
 * @param bytes How many bytes it writes.
 * @param exit_code What it exits with.
 * @param child Set to its process.
 * @return The pipe's reading end, or NULL.
 */
#ifndef _WIN32
static FILE* producer_pipe(size_t bytes, int exit_code, pid_t* child)
{
    int fds[2];

    if (pipe(fds) != 0)
    {
        return NULL;
    }

    *child = fork();

    if (*child == 0)
    {
        unsigned char chunk[997];
        close(fds[0]);

        for (size_t offset = 0; offset < bytes;)
        {
            size_t amount = bytes - offset;

            if (amount > sizeof(chunk))
            {
                amount = sizeof(chunk);
            }

            for (size_t i = 0; i < amount; ++i)
            {
                chunk[i] = pixel_byte(offset + i);
            }

            if (write_bytes(fds[1], chunk, amount) != 0)
            {
                _exit(90);
            }

            offset += amount;
        }

        close(fds[1]);
        _exit(exit_code);
    }

    close(fds[1]);

    if (*child < 0)
    {
        close(fds[0]);
        return NULL;
    }

    return fdopen(fds[0], "r");
}

typedef struct FaultStream
{
    size_t offset;

    size_t fail_at;

    int error;

    int failed;
} FaultStream;

/**
 * @brief A frame's stream that fails once, with the fault's error, at the
 *        fault's offset.
 * @param cookie The FaultStream.
 * @param buffer Where to read to.
 * @param bytes How many bytes.
 * @return How many it read, or -1.
 */
static ssize_t read_fault(void* cookie, char* buffer, size_t bytes)
{
    FaultStream* fault = cookie;

    if (!fault->failed && fault->offset == fault->fail_at)
    {
        fault->failed = 1;
        errno = fault->error;
        return -1;
    }

    if (fault->offset == FRAME_BYTES)
    {
        return 0;
    }

    if (bytes > FRAME_BYTES - fault->offset)
    {
        bytes = FRAME_BYTES - fault->offset;
    }

    if (!fault->failed && bytes > fault->fail_at - fault->offset)
    {
        bytes = fault->fail_at - fault->offset;
    }

    for (size_t i = 0; i < bytes; ++i)
    {
        buffer[i] = (char)pixel_byte(fault->offset + i);
    }

    fault->offset += bytes;
    return (ssize_t)bytes;
}

#ifdef __APPLE__
/**
 * @brief read_fault in the shape macOS's funopen() wants.
 * @param cookie The FaultStream.
 * @param buffer Where the bytes go.
 * @param bytes How many to read.
 * @return Bytes read, or -1 with errno set.
 */
static int read_fault_funopen(void* cookie, char* buffer, int bytes)
{
    return (int)read_fault(cookie, buffer, (size_t)bytes);
}
#endif

/**
 * @brief Check reading a frame through a fault: an interruption is read
 *        through, and any other error fails the read.
 * @param offset Where the fault is.
 * @param error The error.
 * @return 0. A failed check stops the test.
 */
static int check_fault(size_t offset, int error)
{
    FaultStream fault = {0, offset, error, 0};
#ifdef __APPLE__
    FILE* stream = funopen(&fault, read_fault_funopen, NULL, NULL, NULL);
#else
    cookie_io_functions_t functions = {.read = read_fault};
    FILE* stream = fopencookie(&fault, "r", functions);
#endif
    CHECK(stream != NULL);
    LomMovieReadResult result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
    CHECK(fault.failed);

    if (error == EINTR)
    {
        CHECK(result.bytes == sizeof(frame) && result.error == 0 && !result.eof);
        CHECK(check_frame(sizeof(frame)));
        result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
        CHECK(result.bytes == 0 && result.eof && result.error == 0);
    }
    else
    {
        CHECK(result.bytes == offset && !result.eof);
        CHECK(result.error == (error == 0 ? EIO : error));
        CHECK(check_frame(offset));
    }

    CHECK(fclose(stream) == 0);
    return 0;
}
#endif

/**
 * @brief Stand in for ffmpeg, by the input it's given: write three frames of
 *        the test pattern, fail, or write until it's stopped.
 * @param argc The arguments' count.
 * @param argv The arguments ffmpeg would get.
 * @return 0, 7 for the failing input, or 61 to 66 when it's run wrong or can't
 *        write.
 */
static int decoder_child(int argc, char** argv)
{
    const char* input = NULL;
    const char* limit = NULL;
    const char* scale = NULL;

    for (int i = 1; i + 1 < argc; ++i)
    {
        if (strcmp(argv[i], "-i") == 0)
        {
            input = argv[i + 1];
        }

        if (strcmp(argv[i], "-frames:v") == 0)
        {
            limit = argv[i + 1];
        }

        if (strcmp(argv[i], "-vf") == 0)
        {
            scale = argv[i + 1];
        }
    }

    if (input == NULL)
    {
        return 61;
    }

#ifdef _WIN32
    _setmode(STDOUT_FILENO, _O_BINARY);
#endif

    if (strcmp(input, "failed") == 0)
    {
        return 7;
    }

    if (strcmp(input, "cancel") == 0)
    {
        for (;;)
        {
            if (write_bytes(STDOUT_FILENO, frame, sizeof(frame)) != 0)
            {
                return 62;
            }
        }
    }

    if (limit == NULL || strcmp(limit, "3") != 0)
    {
        return 63;
    }

    if (strcmp(input, "complete") != 0 && strcmp(input, "sized") != 0 && strcmp(input, tricky_input) != 0)
    {
        return 66;
    }

    size_t bytes = strcmp(input, "sized") == 0 ? 176u * 176u * 3u : sizeof(frame);

    if (scale == NULL || strcmp(scale, bytes == sizeof(frame) ? "scale=320:240" : "scale=176:176") != 0)
    {
        return 65;
    }

    for (size_t i = 0; i < bytes; ++i)
    {
        frame[i] = pixel_byte(i);
    }

    for (int i = 0; i < 3; ++i)
    {
        if (write_bytes(STDOUT_FILENO, frame, bytes) != 0)
        {
            return 64;
        }
    }

    return 0;
}

#ifdef _WIN32
#define DECODER_GONE(child) lom_test_process_exited((long long)(child))
#define EXITED_WITH(status, code) ((status) == (code))
#define CANCELLED(status) ((status) == 1)
#else
#define DECODER_GONE(child) (waitpid((child), NULL, WNOHANG) == -1 && errno == ECHILD)
#define EXITED_WITH(status, code) (WIFEXITED(status) && WEXITSTATUS(status) == (code))
#define CANCELLED(status) (WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM)
#endif

/**
 * @brief Check the decoder runs the ffmpeg on the PATH, with arguments that
 *        survive spaces and quotes, and reads its frames, its failure, and a
 *        cancel.
 * @return 0. A failed check stops the test.
 */
static int spawn_checks(void)
{
#ifdef _WIN32
    char directory[4096], decoder_path[4096 + 16], executable[4096];
#else
    char directory[] = "/tmp/lom-decoder-test-XXXXXX";
    char decoder_path[sizeof(directory) + 8];
    char executable[4096];
#endif
    const char* old_path = getenv("PATH");
    char* saved_path = old_path ? strdup(old_path) : NULL;
    pid_t child = -1;
    FILE* stream;
    LomMovieReadResult read_result;
    LomMovieCloseResult close_result;
#if defined(_WIN32)
    CHECK(lom_test_executable_path(executable, sizeof(executable)) == 0);
    CHECK(lom_test_make_temp_folder(directory, sizeof(directory), "lom-decoder-test") != NULL);
    snprintf(decoder_path, sizeof(decoder_path), "%s\\ffmpeg.exe", directory);
    CHECK(lom_test_copy_file(executable, decoder_path) == 0);
    size_t path_bytes = strlen(directory) + 2 + (saved_path ? strlen(saved_path) : 0);
    char* fake_path = malloc(path_bytes);
    CHECK(fake_path != NULL);
    snprintf(fake_path, path_bytes, "%s;%s", directory, saved_path ? saved_path : "");
    CHECK(setenv("PATH", fake_path, 1) == 0);
    free(fake_path);
    CHECK(setenv("LOM_MOVIE_TEST_DECODER", "1", 1) == 0);
#elif defined(__APPLE__)
    uint32_t capacity = sizeof(executable);
    CHECK(_NSGetExecutablePath(executable, &capacity) == 0);
#else
    ssize_t size = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    CHECK(size > 0 && (size_t)size < sizeof(executable) - 1);
    executable[size] = '\0';
#endif
#ifndef _WIN32
    CHECK(mkdtemp(directory) != NULL);
    snprintf(decoder_path, sizeof(decoder_path), "%s/ffmpeg", directory);
    CHECK(symlink(executable, decoder_path) == 0);
    CHECK(setenv("PATH", directory, 1) == 0);
    CHECK(setenv("LOM_MOVIE_TEST_DECODER", "1", 1) == 0);
#endif

    stream = lom_movie_decoder_open("complete", 3, &child);
    CHECK(stream != NULL);

    for (int i = 0; i < 3; ++i)
    {
        read_result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
        CHECK(read_result.bytes == sizeof(frame) && !read_result.eof && !read_result.error);
        CHECK(check_frame(sizeof(frame)));
    }

    read_result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
    CHECK(read_result.bytes == 0 && read_result.eof && !read_result.error);
    close_result = lom_movie_decoder_close(stream, child, 0);
    CHECK(lom_movie_decoder_succeeded(close_result));
    CHECK(DECODER_GONE(child));

    stream = lom_movie_decoder_open(tricky_input, 3, &child);
    CHECK(stream != NULL);

    for (int i = 0; i < 3; ++i)
    {
        read_result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
        CHECK(read_result.bytes == sizeof(frame) && !read_result.error);
    }

    CHECK(lom_movie_decoder_succeeded(lom_movie_decoder_close(stream, child, 0)));

    stream = lom_movie_decoder_open_sized("sized", 3, 176, 176, &child);
    CHECK(stream != NULL);

    for (int i = 0; i < 3; ++i)
    {
        read_result = lom_movie_decoder_read_frame(stream, frame, 176u * 176u * 3u);
        CHECK(read_result.bytes == 176u * 176u * 3u && !read_result.error);
        CHECK(check_frame(read_result.bytes));
    }

    read_result = lom_movie_decoder_read_frame(stream, frame, 1);
    CHECK(read_result.bytes == 0 && read_result.eof && !read_result.error);
    CHECK(lom_movie_decoder_succeeded(lom_movie_decoder_close(stream, child, 0)));
    CHECK(lom_movie_decoder_open_sized("invalid", 3, 0, 176, &child) == NULL && errno == EINVAL);

    stream = lom_movie_decoder_open("failed", 0, &child);
    CHECK(stream != NULL);
    read_result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
    CHECK(read_result.bytes == 0 && read_result.eof);
    close_result = lom_movie_decoder_close(stream, child, 0);
    CHECK(!lom_movie_decoder_succeeded(close_result));
    CHECK(!close_result.wait_error && EXITED_WITH(close_result.wait_status, 7));

    stream = lom_movie_decoder_open("cancel", 0, &child);
    CHECK(stream != NULL);
    read_result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
    CHECK(read_result.bytes == sizeof(frame) && !read_result.error);
    close_result = lom_movie_decoder_close(stream, child, 1);
    CHECK(!close_result.wait_error && !close_result.stream_error);
    CHECK(CANCELLED(close_result.wait_status));
    CHECK(DECODER_GONE(child));

    CHECK(unlink(decoder_path) == 0);
#ifdef _WIN32
    CHECK(setenv("PATH", directory, 1) == 0);
#endif
    errno = 0;
    CHECK(lom_movie_decoder_open("missing", 0, &child) == NULL);
    CHECK(errno == ENOENT);
    CHECK(rmdir(directory) == 0);
    CHECK(unsetenv("LOM_MOVIE_TEST_DECODER") == 0);

    if (saved_path)
    {
        CHECK(setenv("PATH", saved_path, 1) == 0);
        free(saved_path);
    }
    else
    {
        CHECK(unsetenv("PATH") == 0);
    }

    return 0;
}

/**
 * @brief Find this program's file.
 * @param path Set to it.
 * @param size Bytes at @p path.
 * @return 0, or -1.
 */
#ifndef _WIN32
static int program_path(char* path, size_t size)
{
#ifdef __APPLE__
    char link[4096];
    uint32_t capacity = sizeof(link);

    if (_NSGetExecutablePath(link, &capacity) != 0 || realpath(link, path) == NULL)
    {
        return -1;
    }

    (void)size;
#else
    ssize_t length = readlink("/proc/self/exe", path, size - 1);

    if (length <= 0 || (size_t)length >= size - 1)
    {
        return -1;
    }

    path[length] = '\0';
#endif
    return 0;
}

/**
 * @brief Copy a program, keeping it runnable.
 * @param from The program.
 * @param to The copy.
 * @return 0, or -1.
 */
static int copy_program(const char* from, const char* to)
{
    FILE* in = fopen(from, "rb");
    FILE* out = in ? fopen(to, "wb") : NULL;
    char buffer[65536];
    size_t bytes;
    int failed = in == NULL || out == NULL;

    while (!failed && (bytes = fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        failed = fwrite(buffer, 1, bytes, out) != bytes;
    }

    failed |= in != NULL && ferror(in);

    if (out != NULL && fclose(out) != 0)
    {
        failed = 1;
    }

    if (in != NULL)
    {
        fclose(in);
    }

    return failed || chmod(to, 0755) != 0 ? -1 : 0;
}

/**
 * @brief Open a movie with the decoder next to the program and read its three
 *        frames.
 * @return 0. A failed check stops the test.
 */
static int bundled_child(void)
{
    pid_t child = -1;
    FILE* stream;
    LomMovieReadResult read_result;

    CHECK(setenv("LOM_MOVIE_TEST_DECODER", "1", 1) == 0);
    stream = lom_movie_decoder_open("complete", 3, &child);
    CHECK(stream != NULL);

    for (int i = 0; i < 3; ++i)
    {
        read_result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
        CHECK(read_result.bytes == sizeof(frame) && !read_result.error);
        CHECK(check_frame(sizeof(frame)));
    }

    CHECK(lom_movie_decoder_succeeded(lom_movie_decoder_close(stream, child, 0)));
    return 0;
}

/**
 * @brief Check the decoder prefers the ffmpeg next to the program, and falls
 *        back to the PATH when there isn't one.
 * @return 0. A failed check stops the test.
 */
static int bundled_checks(void)
{
    char directory[] = "/tmp/lom-decoder-bundled-XXXXXX";
    char runner[sizeof(directory) + 8], decoder[sizeof(directory) + 8], empty[sizeof(directory) + 8];
    char executable[4096];
    int status;

    CHECK(program_path(executable, sizeof(executable)) == 0);
    CHECK(mkdtemp(directory) != NULL);
    snprintf(runner, sizeof(runner), "%s/runner", directory);
    snprintf(decoder, sizeof(decoder), "%s/ffmpeg", directory);
    snprintf(empty, sizeof(empty), "%s/empty", directory);
    CHECK(copy_program(executable, runner) == 0);
    CHECK(symlink(executable, decoder) == 0);
    CHECK(mkdir(empty, 0700) == 0);

    pid_t pid = fork();

    if (pid == 0)
    {
        if (setenv("PATH", empty, 1) == 0 && setenv("LOM_MOVIE_TEST_BUNDLED", "1", 1) == 0)
        {
            execl(runner, runner, (char*)NULL);
        }

        _exit(127);
    }

    CHECK(pid > 0);

    while (waitpid(pid, &status, 0) < 0)
    {
        CHECK(errno == EINTR);
    }

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(unlink(runner) == 0 && unlink(decoder) == 0 && rmdir(empty) == 0 && rmdir(directory) == 0);
    return 0;
}
#endif

int main(int argc, char** argv)
{
    if (getenv("LOM_MOVIE_TEST_DECODER"))
    {
        return decoder_child(argc, argv);
    }

#ifndef _WIN32
    if (getenv("LOM_MOVIE_TEST_BUNDLED"))
    {
        return bundled_child();
    }
#endif

#ifdef _WIN32
    CHECK(spawn_checks() == 0);
    puts("movie decoder: frames, exit status, quoting, cancellation and a missing decoder passed");
    return 0;
#else
    pid_t child;
    FILE* stream = producer_pipe(sizeof(frame), 0, &child);
    CHECK(stream != NULL);
    LomMovieReadResult result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
    CHECK(result.bytes == sizeof(frame) && !result.eof && !result.error);
    CHECK(check_frame(sizeof(frame)));
    result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
    CHECK(result.bytes == 0 && result.eof && !result.error);
    CHECK(lom_movie_decoder_succeeded(lom_movie_decoder_close(stream, child, 0)));

    stream = producer_pipe(65536, 7, &child);
    CHECK(stream != NULL);
    result = lom_movie_decoder_read_frame(stream, frame, sizeof(frame));
    CHECK(result.bytes == 65536 && result.eof && !result.error);
    CHECK(check_frame(65536));
    LomMovieCloseResult close_result = lom_movie_decoder_close(stream, child, 0);
    CHECK(!lom_movie_decoder_succeeded(close_result));
    CHECK(!close_result.wait_error && EXITED_WITH(close_result.wait_status, 7));

    CHECK(check_fault(65536, EINTR) == 0);
    CHECK(check_fault(0, EINTR) == 0);
    CHECK(check_fault(65536, EIO) == 0);
    CHECK(check_fault(0, EIO) == 0);
    CHECK(check_fault(0, 0) == 0);
    CHECK(spawn_checks() == 0);
    CHECK(bundled_checks() == 0);
    puts("movie decoder: fragmented frames, interrupted reads, EOF, read failures, exit status, cancellation and a bundled decoder passed");
    return 0;
#endif
}
