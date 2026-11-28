#define _GNU_SOURCE
#include "common/pad.h"
static SCDRegs fixture_controller;
#undef SCD_REGS
#define SCD_REGS (&fixture_controller)
#include "movie_native.c"
#include "../support/lom_check.h"
#include "../support/platform/lom_test_posix.h"
#include <errno.h>
#include <signal.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
int lom_test_process_exited(long long id);
FILE* lom_test_unreadable_stream(void);
int lom_test_executable_path(char* path, size_t size);
int lom_test_temporary_folder(char* path, size_t size);
int lom_test_copy_file(const char* from, const char* to);
#else
#include <sys/wait.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static unsigned uploads, waits, cleaned, close_calls, cancel_calls;

static unsigned final_eof_reads;

static int skip_input;

static const char* fixture_source;

static LomMovieCloseResult last_close;

extern LomMovieCloseResult __real_lom_movie_decoder_close(FILE*, pid_t, int);

/**
 * @brief Count the decoder's closes and cancels, and check its process is gone
 *        once it's closed.
 * @param stream The decoder's output.
 * @param child Its process.
 * @param cancel Nonzero when the movie was cut short.
 * @return What the real close returns.
 */
LomMovieCloseResult __wrap_lom_movie_decoder_close(FILE* stream, pid_t child, int cancel)
{
    ++close_calls;
    cancel_calls += (cancel != 0);
    last_close = __real_lom_movie_decoder_close(stream, child, cancel);
#ifdef _WIN32
    CHECK_COUNTED(lom_test_process_exited((long long)child));
#else
    CHECK_COUNTED(waitpid(child, NULL, WNOHANG) == -1 && errno == ECHILD);
#endif
    return last_close;
}

typedef struct
{
    FILE* source;

    int injected;

    int error;
} BoundaryFault;

/**
 * @brief A read that fails once with the fault's error, then reads the real
 *        stream, to test a read interrupted at a frame's boundary.
 * @param cookie The BoundaryFault.
 * @param output Where to read to.
 * @param bytes How many bytes.
 * @return How many it read, or -1.
 */
static ssize_t boundary_read(void* cookie, char* output, size_t bytes)
{
    BoundaryFault* fault = cookie;

    if (!fault->injected)
    {
        fault->injected = 1;
        errno = fault->error;
        return -1;
    }

    size_t got = fread(output, 1, bytes, fault->source);

    if (!got && ferror(fault->source))
    {
        return -1;
    }

    return (ssize_t)got;
}

#ifdef __APPLE__
/**
 * @brief boundary_read in the shape macOS's funopen() wants.
 * @param cookie The BoundaryFault.
 * @param output Where the bytes go.
 * @param bytes How many to read.
 * @return Bytes read, or -1 with errno set.
 */
static int boundary_read_funopen(void* cookie, char* output, int bytes)
{
    return (int)boundary_read(cookie, output, (size_t)bytes);
}
#endif

extern LomMovieReadResult __real_lom_movie_decoder_read_frame(FILE*, void*, size_t);

/**
 * @brief Read a frame, putting in the case's fault on the read after the last
 *        frame that checks for the end.
 * @param stream The decoder's output.
 * @param output Where to read to.
 * @param bytes How many bytes.
 * @return What the read gives, or the fault.
 */
LomMovieReadResult __wrap_lom_movie_decoder_read_frame(FILE* stream, void* output, size_t bytes)
{
    if (bytes == 1)
    {
        ++final_eof_reads;

#ifdef _WIN32
        if (!strcmp(fixture_source, "boundary-eio"))
        {
            FILE* injected = lom_test_unreadable_stream();

            if (!injected)
            {
                CHECK_COUNTED(0);
                return (LomMovieReadResult){0, 0, ENOMEM};
            }

            LomMovieReadResult result = __real_lom_movie_decoder_read_frame(injected, output, bytes);
            CHECK_COUNTED(result.error != 0 && !fclose(injected));
            return result;
        }
#else
        if (!strcmp(fixture_source, "boundary-eintr") || !strcmp(fixture_source, "boundary-eio"))
        {
            BoundaryFault fault = {stream, 0, !strcmp(fixture_source, "boundary-eintr") ? EINTR : EIO};
#ifdef __APPLE__
            FILE* injected = funopen(&fault, boundary_read_funopen, NULL, NULL, NULL);
#else
            cookie_io_functions_t functions = {.read = boundary_read};
            FILE* injected = fopencookie(&fault, "r", functions);
#endif

            if (!injected)
            {
                CHECK_COUNTED(0);
                return (LomMovieReadResult){0, 0, ENOMEM};
            }

            LomMovieReadResult result = __real_lom_movie_decoder_read_frame(injected, output, bytes);
            CHECK_COUNTED(fault.injected && !fclose(injected));
            return result;
        }
#endif
    }

    return __real_lom_movie_decoder_read_frame(stream, output, bytes);
}

int lom_native_cd_resolve_resource_path(int resource, char* path, size_t bytes)
{
    CHECK_COUNTED(resource == 0x16A1);
    return snprintf(path, bytes, "%s", fixture_source) > 0 ? 0 : -1;
}

/**
 * @brief No Remaster here, so the disc's movie plays.
 * @param movie Which movie.
 * @param path Unused.
 * @param size Unused.
 * @return 0.
 */
int lom_native_remaster_movie_path(unsigned movie, char* path, unsigned long size)
{
    (void)movie;
    (void)path;
    (void)size;
    return 0;
}

/**
 * @brief No Remaster here, so no sound of its own.
 * @param movie Which movie.
 * @param file Unused.
 * @return 0.
 */
int lom_native_remaster_movie_sound(unsigned movie, FILE* file)
{
    (void)movie;
    (void)file;
    return 0;
}

/**
 * @brief Only a Remaster movie wants a temporary file here, and there isn't one.
 * @param path Unused.
 * @param path_size Unused.
 * @param file_out Unused.
 * @return -1.
 */
int lom_movie_temp_file(char* path, size_t path_size, FILE** file_out)
{
    (void)path;
    (void)path_size;
    (void)file_out;
    return -1;
}

/**
 * @brief No Remaster here.
 * @param movie Which movie.
 * @return NULL.
 */
const char* lom_native_remaster_movie_file(unsigned movie)
{
    (void)movie;
    return NULL;
}

int lom_movie_prepare_file(const char* path, LomPreparedMovie* prepared)
{
    memset(prepared, 0, sizeof(*prepared));
    snprintf(prepared->video_path, sizeof(prepared->video_path), "%s", path);
    prepared->width = 320;
    prepared->height = 240;
    return 0;
}

void lom_movie_cleanup_source(LomPreparedMovie* prepared)
{
    CHECK_COUNTED(prepared->audio_pcm == NULL);
    ++cleaned;
}

void update_controllers(void)
{
    fixture_controller.device_type = 0;
    fixture_controller.pressed_buttons = skip_input ? PAD_BTN_R1 : 0;
}

/**
 * @brief A VSync that counts the waits.
 * @param mode Must be 0.
 * @return 0.
 */
int VSync(int mode)
{
    CHECK_COUNTED(mode == 0);
    ++waits;
    return 0;
}

/**
 * @brief A DrawSync that does nothing.
 * @param mode Must be 0.
 * @return 0.
 */
int DrawSync(int mode)
{
    CHECK_COUNTED(mode == 0);
    return 0;
}

/**
 * @brief A SetDispMask that checks its argument.
 * @param mask 0 or 1.
 */
void SetDispMask(int mask)
{
    CHECK_COUNTED(mask == 0 || mask == 1);
}

/**
 * @brief Psy-Q's SetDefDispEnv, for the display area only.
 * @param env Set up.
 * @param x The area's left.
 * @param y Its top.
 * @param width Its width.
 * @param height Its height.
 * @return @p env.
 */
DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int width, int height)
{
    memset(env, 0, sizeof(*env));
    env->disp = (RECT){x, y, width, height};
    return env;
}

/**
 * @brief A PutDispEnv that checks the movie shows the half of VRAM it last
 *        drew.
 * @param env The display.
 * @return @p env.
 */
DISPENV* PutDispEnv(DISPENV* env)
{
    CHECK_COUNTED(env->disp.x == 0 && env->disp.y == (int)(uploads % 2 ? 0 : 240));
    return env;
}

/**
 * @brief A LoadImage that checks each frame's pixels are the decoder's, turned
 *        into 15-bit color, in the half of VRAM it draws.
 * @param rect Where they go.
 * @param data The pixels.
 * @return 0.
 */
int LoadImage(RECT* rect, LomPsyzULong* data)
{
    CHECK_COUNTED(rect->x == 0 && rect->y == (int)((uploads % 2) * 240) && rect->w == 320 && rect->h == 240);
    u16* pixels = (u16*)data;

    for (unsigned i = 0; i < 320u * 240u; ++i)
    {
        unsigned red = ((3 * i + uploads * 17) & 255u) >> 3;
        unsigned green = ((3 * i + 1 + uploads * 17) & 255u) >> 3;
        unsigned blue = ((3 * i + 2 + uploads * 17) & 255u) >> 3;
        CHECK_COUNTED(pixels[i] == (u16)(0x8000 | red | (green << 5) | (blue << 10)));
    }

    ++uploads;
    return 0;
}

/**
 * @brief Fails the test: a movie without sound shouldn't start SDL's audio.
 * @param flags Ignored.
 * @return false.
 */
bool SDL_InitSubSystem(SDL_InitFlags flags)
{
    (void)flags;
    CHECK_COUNTED(0);
    return false;
}

/**
 * @brief Fails the test: a movie without sound shouldn't ask about SDL's audio.
 * @param flags Ignored.
 * @return 0.
 */
SDL_InitFlags SDL_WasInit(SDL_InitFlags flags)
{
    (void)flags;
    CHECK_COUNTED(0);
    return 0;
}

/**
 * @brief SDL's GetError, for the messages.
 * @return Why there's no SDL.
 */
const char* SDL_GetError(void)
{
    return "CPU fixture: no SDL initialized";
}

/**
 * @brief Fails the test: a movie without sound shouldn't open an audio stream.
 * @param id Ignored.
 * @param spec Ignored.
 * @param callback Ignored.
 * @param userdata Ignored.
 * @return NULL.
 */
SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID id, const SDL_AudioSpec* spec, SDL_AudioStreamCallback callback, void* userdata)
{
    (void)id;
    (void)spec;
    (void)callback;
    (void)userdata;
    CHECK_COUNTED(0);
    return NULL;
}

/**
 * @brief Fails the test, as SDL_OpenAudioDeviceStream does.
 * @param stream Ignored.
 * @return false.
 */
bool SDL_ResumeAudioStreamDevice(SDL_AudioStream* stream)
{
    (void)stream;
    CHECK_COUNTED(0);
    return false;
}

/**
 * @brief Fails the test, as SDL_OpenAudioDeviceStream does.
 * @param stream Ignored.
 */
void SDL_DestroyAudioStream(SDL_AudioStream* stream)
{
    (void)stream;
    CHECK_COUNTED(0);
}

/**
 * @brief Fails the test: a movie without sound shouldn't pause the game's.
 */
void Psyz_AudioPause(void)
{
    CHECK_COUNTED(0);
}

/**
 * @brief Fails the test: a movie without sound shouldn't pause the game's.
 */
void Psyz_AudioUnpause(void)
{
    CHECK_COUNTED(0);
}

LomMovieAudioGains lom_native_akao_movie_audio_gains(void)
{
    CHECK_COUNTED(0);
    return (LomMovieAudioGains){0};
}

LomMovieAudioOutput* lom_movie_audio_output_create(SDL_AudioStream* stream, size_t frames, LomMovieAudioGains gains)
{
    (void)stream;
    (void)frames;
    (void)gains;
    CHECK_COUNTED(0);
    return NULL;
}

int lom_movie_audio_output_failed(LomMovieAudioOutput* output)
{
    (void)output;
    CHECK_COUNTED(0);
    return 1;
}

int lom_movie_audio_output_publish(LomMovieAudioOutput* output, const void* pcm, size_t frames)
{
    (void)output;
    (void)pcm;
    (void)frames;
    CHECK_COUNTED(0);
    return -1;
}

int lom_movie_audio_output_set_volume(LomMovieAudioOutput* output, unsigned level)
{
    (void)output;
    (void)level;
    CHECK_COUNTED(0);
    return -1;
}

int lom_movie_audio_output_destroy(LomMovieAudioOutput* output)
{
    (void)output;
    CHECK_COUNTED(0);
    return -1;
}

/**
 * @brief Write all of a buffer to standard output, through interruptions.
 * @param data The bytes.
 * @param bytes How many.
 * @return 0, or -1.
 */
static int write_all(const void* data, size_t bytes)
{
    size_t offset = 0;

    while (offset < bytes)
    {
        ssize_t n = write(STDOUT_FILENO, (const unsigned char*)data + offset, bytes - offset);

        if (n < 0 && errno == EINTR)
        {
            continue;
        }

        if (n <= 0)
        {
            return -1;
        }

        offset += (size_t)n;
    }

    return 0;
}

/**
 * @brief Stand in for ffmpeg: write the case's frames of a known pattern, for a
 *        decoder asked for 3 frames.
 * @param argc The arguments' count.
 * @param argv The arguments ffmpeg would get.
 * @return 0, 7 for the case whose decoder fails at the end, or 81 or 82 when
 *        it's run wrong or can't write.
 */
static int decoder_child(int argc, char** argv)
{
    const char* source = NULL;
    unsigned cap = 0;

    for (int i = 1; i + 1 < argc; ++i)
    {
        if (!strcmp(argv[i], "-i"))
        {
            source = argv[i + 1];
        }

        if (!strcmp(argv[i], "-frames:v"))
        {
            cap = (unsigned)strtoul(argv[i + 1], NULL, 10);
        }
    }

    if (!source || cap != 3)
    {
        return 81;
    }

    unsigned char bytes[320 * 240 * 3];
    unsigned frames = 3;
#ifdef _WIN32
    _setmode(STDOUT_FILENO, _O_BINARY);
#endif

    if (!strcmp(source, "skip") || !strcmp(source, "extra"))
    {
        frames = 100000;
    }

    for (unsigned f = 0; f < frames; ++f)
    {
        for (size_t i = 0; i < sizeof(bytes); ++i)
        {
            bytes[i] = (unsigned char)(i + f * 17);
        }

        if (write_all(bytes, sizeof(bytes)))
        {
            return 82;
        }
    }

    return !strcmp(source, "failed-final") ? 7 : 0;
}

/**
 * @brief Play movie 1 with the fake decoder in one case, and check how it
 *        ended.
 * @param source The case.
 * @param skip Nonzero to press a button that skips it.
 * @param expected The status it should end with.
 * @param expected_frames The frames it should show.
 * @param expected_cancels How many times the decoder should be cancelled.
 * @param expected_eof_reads How many reads after the last frame should check
 *        for the end.
 */
static void check_play(const char* source, int skip, LomNativeMovieStatus expected, unsigned expected_frames, unsigned expected_cancels,
                       unsigned expected_eof_reads)
{
    fixture_source = source;
    skip_input = skip;
    uploads = waits = cleaned = close_calls = cancel_calls = final_eof_reads = 0;
    LomNativeMovieStatus status = lom_native_movie_play(1);
    printf("case=%s status=%d frames=%u cancel=%u EOF-check=%u wait=0x%X\n", source, status, uploads, cancel_calls, final_eof_reads, last_close.wait_status);
    CHECK_COUNTED(status == expected);
    CHECK_COUNTED(uploads == expected_frames && lom_native_movie_presented_frames() == expected_frames);
    CHECK_COUNTED(waits == expected_frames * 4 && cleaned == 1 && close_calls == 1);
    CHECK_COUNTED(cancel_calls == expected_cancels && final_eof_reads == expected_eof_reads);
    CHECK_COUNTED(last_close.wait_error == 0 && last_close.stream_error == 0);

#ifdef _WIN32
    if (!strcmp(source, "complete"))
    {
        CHECK_COUNTED(last_close.wait_status == 0);
    }

    if (!strcmp(source, "failed-final"))
    {
        CHECK_COUNTED(last_close.wait_status == 7);
    }

    if (skip)
    {
        CHECK_COUNTED(last_close.wait_status == 1);
    }
#else
    if (!strcmp(source, "complete"))
    {
        CHECK_COUNTED(WIFEXITED(last_close.wait_status) && WEXITSTATUS(last_close.wait_status) == 0);
    }

    if (!strcmp(source, "failed-final"))
    {
        CHECK_COUNTED(WIFEXITED(last_close.wait_status) && WEXITSTATUS(last_close.wait_status) == 7);
    }

    if (skip)
    {
        CHECK_COUNTED(WIFSIGNALED(last_close.wait_status) && WTERMSIG(last_close.wait_status) == SIGTERM);
    }
#endif
}

int main(int argc, char** argv)
{
    if (getenv("LOM_MOVIE_COMPLETION_CHILD"))
    {
        return decoder_child(argc, argv);
    }

#ifdef _WIN32
    char root[4096 + 32], binary[4096], ffmpeg[4096 + 64], folder[4096];
#else
    char root[] = "/tmp/lom-completion-test-XXXXXX", binary[4096], ffmpeg[128];
#endif
    const char* current_path = getenv("PATH");
    char* saved_path = current_path ? strdup(current_path) : NULL;
#if defined(_WIN32)
    if (lom_test_executable_path(binary, sizeof(binary)) || lom_test_temporary_folder(folder, sizeof(folder)))
    {
        return 2;
    }

    snprintf(root, sizeof(root), "%slom-completion-test-XXXXXX", folder);
#elif defined(__APPLE__)
    uint32_t capacity = sizeof(binary);

    if (_NSGetExecutablePath(binary, &capacity) != 0)
    {
        return 2;
    }
#else
    ssize_t n = readlink("/proc/self/exe", binary, sizeof(binary) - 1);

    if (n <= 0 || (size_t)n >= sizeof(binary) - 1)
    {
        return 2;
    }

    binary[n] = '\0';
#endif

    if (!mkdtemp(root))
    {
        return 2;
    }

#ifdef _WIN32
    snprintf(ffmpeg, sizeof(ffmpeg), "%s\\ffmpeg.exe", root);
    size_t path_bytes = strlen(root) + 2 + (saved_path ? strlen(saved_path) : 0);
    char* fake_path = malloc(path_bytes);

    if (fake_path == NULL || lom_test_copy_file(binary, ffmpeg))
    {
        return 2;
    }

    snprintf(fake_path, path_bytes, "%s;%s", root, saved_path ? saved_path : "");

    if (setenv("PATH", fake_path, 1) || setenv("LOM_MOVIE_COMPLETION_CHILD", "1", 1))
    {
        return 2;
    }

    free(fake_path);
#else
    snprintf(ffmpeg, sizeof(ffmpeg), "%s/ffmpeg", root);

    if (symlink(binary, ffmpeg) || setenv("PATH", root, 1) || setenv("LOM_MOVIE_COMPLETION_CHILD", "1", 1))
    {
        return 2;
    }
#endif

    lom_native_movie_set_frame_limit(3);
    check_play("complete", 0, LOM_NATIVE_MOVIE_OK, 3, 0, 1);
    check_play("failed-final", 0, LOM_NATIVE_MOVIE_ERROR, 3, 0, 1);
    check_play("extra", 0, LOM_NATIVE_MOVIE_ERROR, 3, 1, 1);
#ifndef _WIN32
    check_play("boundary-eintr", 0, LOM_NATIVE_MOVIE_OK, 3, 0, 1);
#endif
    check_play("boundary-eio", 0, LOM_NATIVE_MOVIE_ERROR, 3, 1, 1);
    check_play("skip", 1, LOM_NATIVE_MOVIE_SKIPPED, 1, 1, 0);
    lom_native_switch_set_setting(LOM_SWITCH_SKIP_MOVIES, 1);
    uploads = 0;
    CHECK_COUNTED(lom_native_movie_play(1) == LOM_NATIVE_MOVIE_SKIPPED && uploads == 0 && lom_native_movie_presented_frames() == 0);
    lom_native_switch_set_setting(LOM_SWITCH_SKIP_MOVIES, 0);
    CHECK_COUNTED(!unlink(ffmpeg) && !rmdir(root));
    CHECK_COUNTED(!unsetenv("LOM_MOVIE_COMPLETION_CHILD"));

    if (saved_path)
    {
        CHECK_COUNTED(!setenv("PATH", saved_path, 1));
        free(saved_path);
    }
    else
    {
        CHECK_COUNTED(!unsetenv("PATH"));
    }

    printf("movie capped completion: %u checks, %u failures\n", lom_check_count, lom_check_failures);
    return lom_check_failures ? 1 : 0;
}
