#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "lom_native_movie.h"
#include "lom_native_switches.h"
#include "lom_native_game_loop.h"

#include "lom_native_cd.h"

#include "main/controller.h"
#include "common/pad.h"

#define u_long LomPsyzULong
#include <libetc.h>
#include <libgpu.h>
#include <psyz/audio.h>
#undef u_long

#include <SDL3/SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "movie_decoder.h"
#include "movie_source.h"
#include "movie_audio_output.h"

#define LOM_NATIVE_INTRO_MOVIE_RESOURCE 0x16A0
#define LOM_NATIVE_MOVIE_WIDTH 320
#define LOM_NATIVE_MOVIE_HEIGHT 240
#define LOM_NATIVE_MOVIE_FRAME_VSYNCS 4
#define LOM_NATIVE_MOVIE_AUDIO_RATE 44100
#define LOM_NATIVE_MOVIE_AUDIO_CHANNELS 2
#define LOM_NATIVE_MOVIE_AUDIO_PREBUFFER_FRAMES 4
#define LOM_NATIVE_MOVIE_AUDIO_FRAMES_PER_VIDEO_FRAME (LOM_NATIVE_MOVIE_AUDIO_RATE / 15)
#define LOM_NATIVE_MOVIE_AUDIO_BYTES_PER_VIDEO_FRAME (LOM_NATIVE_MOVIE_AUDIO_FRAMES_PER_VIDEO_FRAME * LOM_NATIVE_MOVIE_AUDIO_CHANNELS * sizeof(short))
#define LOM_NATIVE_MOVIE_PATH_MAX 4096
#define LOM_NATIVE_MOVIE_PIXELS ((size_t)LOM_NATIVE_MOVIE_WIDTH * LOM_NATIVE_MOVIE_HEIGHT)
#define LOM_NATIVE_MOVIE_FRAME_BYTES (LOM_NATIVE_MOVIE_PIXELS * 3u)

static unsigned int s_frame_limit;

static unsigned int s_presented_frames;

/**
 * @brief Read the controller and check whether the player is skipping the
 *        movie. The opening skips on any button but the face buttons, entering
 *        Home on R1, R2 or down, and the rest can't be skipped.
 * @param movie_index Which movie.
 * @return 1 when it's skipped.
 */
static int movie_skip_requested(unsigned int movie_index)
{
    u16 buttons;

    update_controllers();

    if (movie_index >= 2)
    {
        return 0;
    }

    if ((SCD_REGS)->device_type >= 3)
    {
        return 0;
    }

    buttons = (SCD_REGS)->pressed_buttons;
    u16 mask = movie_index == 0 ? (u16) ~(PAD_BTN_SQUARE | PAD_BTN_CROSS | PAD_BTN_CIRCLE | PAD_BTN_TRIANGLE) : (PAD_BTN_R2 | PAD_BTN_R1 | PAD_BTN_DOWN);
    return (buttons & mask) != 0;
}

/**
 * @brief Open an audio stream for a movie's sound, starting SDL's audio if it
 *        isn't.
 * @return The stream, or NULL when it can't be opened.
 */
static SDL_AudioStream* open_movie_audio_stream(void)
{
    SDL_AudioSpec spec;
    SDL_AudioStream* stream;

    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        fprintf(stderr, "[lom-native] MOVIE audio init failed: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_zero(spec);
    spec.format = SDL_AUDIO_S16;
    spec.channels = LOM_NATIVE_MOVIE_AUDIO_CHANNELS;
    spec.freq = LOM_NATIVE_MOVIE_AUDIO_RATE;

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);

    if (stream == NULL)
    {
        fprintf(stderr, "[lom-native] MOVIE audio stream failed: %s\n", SDL_GetError());
        return NULL;
    }

    return stream;
}

/**
 * @brief Read one video frame's worth of a movie's sound and queue it to play.
 * @param output Where it plays.
 * @param audio_file The movie's sound, as 16-bit stereo PCM.
 * @param audio_chunk Room for one frame's worth.
 * @return 1 when it queued it, 0 at the end of the sound, or -1 when it fails
 *        or the sound ends partway through a frame.
 */
static int queue_movie_audio_frame(LomMovieAudioOutput* output, FILE* audio_file, unsigned char* audio_chunk)
{
    size_t audio_bytes;

    if (lom_movie_audio_output_failed(output))
    {
        fprintf(stderr, "[lom-native] MOVIE audio output callback failed: %s\n", SDL_GetError());
        return -1;
    }

    audio_bytes = fread(audio_chunk, 1, LOM_NATIVE_MOVIE_AUDIO_BYTES_PER_VIDEO_FRAME, audio_file);

    if (audio_bytes == 0)
    {
        return feof(audio_file) ? 0 : -1;
    }

    if (audio_bytes != LOM_NATIVE_MOVIE_AUDIO_BYTES_PER_VIDEO_FRAME)
    {
        fprintf(stderr, "[lom-native] Movie ended with a partial PCM audio frame (%zu/%zu bytes).\n", audio_bytes,
                (size_t)LOM_NATIVE_MOVIE_AUDIO_BYTES_PER_VIDEO_FRAME);
        return -1;
    }

    if (lom_movie_audio_output_publish(output, audio_chunk, audio_bytes / (2 * sizeof(int16_t))) != 0)
    {
        fprintf(stderr, "[lom-native] MOVIE audio queue failed: %s\n", SDL_GetError());
        return -1;
    }

    return 1;
}

/**
 * @brief Queue the first frames of a movie's sound before its picture starts,
 *        as the PS1's driver does.
 * @param output Where it plays.
 * @param audio_file The movie's sound.
 * @param audio_chunk Room for one frame's worth.
 * @return 0, or -1 when the sound is shorter than that.
 */
static int prebuffer_movie_audio(LomMovieAudioOutput* output, FILE* audio_file, unsigned char* audio_chunk)
{
    unsigned int frame;

    for (frame = 0; frame < LOM_NATIVE_MOVIE_AUDIO_PREBUFFER_FRAMES; frame++)
    {
        if (queue_movie_audio_frame(output, audio_file, audio_chunk) != 1)
        {
            fprintf(stderr, "[lom-native] Movie does not contain the %u audio frames required for AKAO-style priming.\n",
                    LOM_NATIVE_MOVIE_AUDIO_PREBUFFER_FRAMES);
            return -1;
        }
    }

    return 0;
}

void lom_native_movie_set_frame_limit(unsigned int frame_limit)
{
    s_frame_limit = frame_limit;
}

void movie_play(u16 movie_index)
{
    (void)lom_native_movie_play(movie_index);
}

unsigned int lom_native_movie_presented_frames(void)
{
    return s_presented_frames;
}

typedef struct
{
    unsigned int index;
    unsigned int limit;
    LomPreparedMovie prepared;
    FILE* video_pipe;
    pid_t video_child;
    unsigned char* video_frame;
    u16* video_pixels;
    FILE* audio_file;
    SDL_AudioStream* audio_stream;
    LomMovieAudioOutput* audio_output;
    unsigned char audio_chunk[LOM_NATIVE_MOVIE_AUDIO_BYTES_PER_VIDEO_FRAME];
    LomMovieReadResult read_result;
    size_t bytes_read;
    unsigned int frame_count;
    int skipped;
    int audio_error;
} MoviePlayback;

/**
 * @brief Find a movie on the disc, split it into its video and sound, and
 *        start decoding the video.
 * @param movie Filled in.
 * @param movie_index Which movie.
 * @return 0, or -1 when it can't be played (and nothing is left to clean up).
 */
static int open_movie(MoviePlayback* movie, unsigned int movie_index)
{
    static const unsigned int retail_frame_limits[] = {2098, 2473, 1318, 5368, 898};
    char movie_path[LOM_NATIVE_MOVIE_PATH_MAX];

    if (movie_index >= sizeof(retail_frame_limits) / sizeof(retail_frame_limits[0]))
    {
        fprintf(stderr, "[lom-native] Unsupported movie index %u.\n", movie_index);
        return -1;
    }

    movie->index = movie_index;
    movie->limit = retail_frame_limits[movie_index];

    if (s_frame_limit != 0 && s_frame_limit < movie->limit)
    {
        movie->limit = s_frame_limit;
    }

    unsigned int resource = LOM_NATIVE_INTRO_MOVIE_RESOURCE + movie_index;

    if (lom_native_cd_resolve_resource_path(resource, movie_path, sizeof(movie_path)) != 0)
    {
        fprintf(stderr, "[lom-native] Movie resource 0x%X is not mapped.\n", resource);
        return -1;
    }

    if (lom_movie_prepare_file(movie_path, &movie->prepared) != 0)
    {
        return -1;
    }

    movie->video_pipe = lom_movie_decoder_open(movie->prepared.video_path, movie->limit, &movie->video_child);

    if (movie->video_pipe == NULL)
    {
        fprintf(stderr, "[lom-native] Could not start ffmpeg for movie video.\n");
        lom_movie_cleanup_source(&movie->prepared);
        return -1;
    }

    movie->audio_file = movie->prepared.audio_pcm;
    return 0;
}

/**
 * @brief Pause the game's sound and start the movie's: open a stream, set up
 *        its output and queue the first frames. When no stream opens, the movie
 *        plays silent and the game's sound comes back on.
 * @param movie The movie.
 * @return 0, or -1 when the sound can't be set up.
 */
static int start_movie_audio(MoviePlayback* movie)
{
    if (movie->audio_file == NULL)
    {
        return 0;
    }

    Psyz_AudioPause();
    movie->audio_stream = open_movie_audio_stream();

    if (movie->audio_stream == NULL)
    {
        Psyz_AudioUnpause();
        movie->audio_file = NULL;
        return 0;
    }

    size_t capacity = ((size_t)movie->limit + LOM_NATIVE_MOVIE_AUDIO_PREBUFFER_FRAMES) * LOM_NATIVE_MOVIE_AUDIO_FRAMES_PER_VIDEO_FRAME;
    movie->audio_output = lom_movie_audio_output_create(movie->audio_stream, capacity, lom_native_akao_movie_audio_gains());

    if (movie->audio_output == NULL)
    {
        fprintf(stderr, "[lom-native] MOVIE raw audio buffer setup failed.\n");
        SDL_DestroyAudioStream(movie->audio_stream);
        movie->audio_stream = NULL;
        Psyz_AudioUnpause();
        return -1;
    }

    return prebuffer_movie_audio(movie->audio_output, movie->audio_file, movie->audio_chunk);
}

/**
 * @brief Give up on a movie while starting or playing it: free its frames,
 *        stop its sound and bring the game's back, stop the decoder and delete
 *        the split files.
 * @param movie The movie.
 * @return LOM_NATIVE_MOVIE_ERROR.
 */
static LomNativeMovieStatus abandon_movie(MoviePlayback* movie)
{
    free(movie->video_frame);
    free(movie->video_pixels);

    if (movie->audio_output != NULL)
    {
        lom_movie_audio_output_destroy(movie->audio_output);
        Psyz_AudioUnpause();
    }

    lom_movie_decoder_close(movie->video_pipe, movie->video_child, 1);
    lom_movie_cleanup_source(&movie->prepared);
    return LOM_NATIVE_MOVIE_ERROR;
}

/**
 * @brief Turn a decoded RGB24 frame into the PS1's 15-bit pixels.
 * @param rgb The frame, three bytes a pixel.
 * @param pixels Receives the 15-bit pixels, with the mask bit set.
 */
static void convert_movie_frame(const unsigned char* rgb, u16* pixels)
{
    for (size_t i = 0; i < LOM_NATIVE_MOVIE_PIXELS; i++)
    {
        unsigned int red = rgb[i * 3 + 0] >> 3;
        unsigned int green = rgb[i * 3 + 1] >> 3;
        unsigned int blue = rgb[i * 3 + 2] >> 3;
        pixels[i] = (u16)(red | (green << 5) | (blue << 10) | 0x8000);
    }
}

/**
 * @brief Queue the sound of the frame on screen, starting the stream with the
 *        first one.
 * @param movie The movie, which has sound.
 * @param started Whether the stream has started; set when this starts it.
 * @return 0, or -1 when the sound fails.
 */
static int play_movie_audio_frame(MoviePlayback* movie, int* started)
{
    if (queue_movie_audio_frame(movie->audio_output, movie->audio_file, movie->audio_chunk) < 0)
    {
        return -1;
    }

    if (!*started)
    {
        if (!SDL_ResumeAudioStreamDevice(movie->audio_stream))
        {
            fprintf(stderr, "[lom-native] MOVIE audio resume failed: %s\n", SDL_GetError());
            return -1;
        }

        *started = 1;
    }

    return 0;
}

/**
 * @brief Handle the player skipping. The opening, or a movie without sound,
 *        stops at once; the others fade their sound out over a few frames first.
 * @param movie The movie.
 * @param fade The fade's volume, or -1 before it starts.
 * @return 1 to stop playing (skipped, or the sound failed), or 0 to go on.
 */
static int update_movie_skip(MoviePlayback* movie, int* fade)
{
    if (movie_skip_requested(movie->index))
    {
        if (movie->index == 0 || movie->audio_stream == NULL)
        {
            movie->skipped = 1;
            return 1;
        }

        if (*fade < 0)
        {
            *fade = 0x70;
        }
    }

    if (*fade < 0)
    {
        return 0;
    }

    if (lom_movie_audio_output_set_volume(movie->audio_output, (unsigned)*fade) != 0)
    {
        fprintf(stderr, "[lom-native] MOVIE audio matrix update failed: %s\n", SDL_GetError());
        movie->audio_error = 1;
        return 1;
    }

    if (*fade == 0)
    {
        movie->skipped = 1;
        return 1;
    }

    *fade -= 0x10;
    return 0;
}

/**
 * @brief Show the movie a frame at a time, flipping between two pages of VRAM,
 *        with its sound, until it ends, is skipped or reaches its limit.
 * @param movie The movie.
 * @return 0, or -1 when its sound fails partway and the movie has to be abandoned.
 */
static int show_movie(MoviePlayback* movie)
{
    DISPENV display_env[2];
    RECT upload_rect[2];
    int audio_started = 0;
    int skip_fade = -1;
    int page = 0;

    SetDefDispEnv(&display_env[0], 0, 0, LOM_NATIVE_MOVIE_WIDTH, LOM_NATIVE_MOVIE_HEIGHT);
    SetDefDispEnv(&display_env[1], 0, LOM_NATIVE_MOVIE_HEIGHT, LOM_NATIVE_MOVIE_WIDTH, LOM_NATIVE_MOVIE_HEIGHT);
    setRECT(&upload_rect[0], 0, 0, LOM_NATIVE_MOVIE_WIDTH, LOM_NATIVE_MOVIE_HEIGHT);
    setRECT(&upload_rect[1], 0, LOM_NATIVE_MOVIE_HEIGHT, LOM_NATIVE_MOVIE_WIDTH, LOM_NATIVE_MOVIE_HEIGHT);
    SetDispMask(0);

    while ((movie->read_result = lom_movie_decoder_read_frame(movie->video_pipe, movie->video_frame, LOM_NATIVE_MOVIE_FRAME_BYTES)),
           (movie->bytes_read = movie->read_result.bytes) == LOM_NATIVE_MOVIE_FRAME_BYTES && movie->read_result.error == 0)
    {
        convert_movie_frame(movie->video_frame, movie->video_pixels);
        LoadImage(&upload_rect[page], (LomPsyzULong*)movie->video_pixels);
        DrawSync(0);
        PutDispEnv(&display_env[page]);
        SetDispMask(1);

        if (movie->audio_file != NULL && movie->audio_stream != NULL && play_movie_audio_frame(movie, &audio_started) != 0)
        {
            return -1;
        }

        for (int vsync = 0; vsync < LOM_NATIVE_MOVIE_FRAME_VSYNCS; vsync++)
        {
            VSync(0);
        }

        lom_native_frame_done();
        movie->frame_count++;
        s_presented_frames = movie->frame_count;

        if (update_movie_skip(movie, &skip_fade))
        {
            break;
        }

        if (movie->limit != 0 && movie->frame_count >= movie->limit)
        {
            break;
        }

        page = 1 - page;
    }

    return 0;
}

/**
 * @brief Stop the decoder and the movie's sound, and say how the movie ended,
 *        logging what went wrong when it didn't play through or skip cleanly.
 * @param movie The movie, shown.
 * @return LOM_NATIVE_MOVIE_OK, LOM_NATIVE_MOVIE_SKIPPED or LOM_NATIVE_MOVIE_ERROR.
 */
static LomNativeMovieStatus finish_movie(MoviePlayback* movie)
{
    LomMovieCloseResult close_result;
    int invalid_completion = 0;

    free(movie->video_frame);
    free(movie->video_pixels);
    SetDispMask(0);

    if (!movie->audio_error && !movie->skipped && movie->frame_count == movie->limit)
    {
        unsigned char extra;
        LomMovieReadResult ended = lom_movie_decoder_read_frame(movie->video_pipe, &extra, 1);

        if (ended.bytes || ended.error || !ended.eof)
        {
            fprintf(stderr,
                    "[lom-native] Movie RGB24 decoder exceeded or failed at frame limit %u: "
                    "extra bytes=%zu eof=%d read error=%d.\n",
                    movie->limit, ended.bytes, ended.eof, ended.error);
            invalid_completion = 1;
        }
    }

    close_result = lom_movie_decoder_close(movie->video_pipe, movie->video_child, movie->audio_error || movie->skipped || invalid_completion);

    if (movie->audio_stream != NULL)
    {
        if (lom_movie_audio_output_destroy(movie->audio_output) != 0)
        {
            fprintf(stderr, "[lom-native] MOVIE final audio output failed: %s\n", SDL_GetError());
            movie->audio_error = 1;
        }

        Psyz_AudioUnpause();
    }

    lom_movie_cleanup_source(&movie->prepared);

    if (movie->bytes_read != 0 && movie->bytes_read != LOM_NATIVE_MOVIE_FRAME_BYTES)
    {
        fprintf(stderr,
                "[lom-native] Movie ended with a partial RGB24 frame (%zu/%zu bytes) after %u frames: "
                "eof=%d read error=%d (%s), ffmpeg wait status=0x%X wait error=%d close error=%d.\n",
                movie->bytes_read, (size_t)LOM_NATIVE_MOVIE_FRAME_BYTES, movie->frame_count, movie->read_result.eof, movie->read_result.error,
                movie->read_result.error ? strerror(movie->read_result.error) : "-", close_result.wait_status, close_result.wait_error,
                close_result.stream_error);
        return LOM_NATIVE_MOVIE_ERROR;
    }

    if (movie->read_result.error != 0)
    {
        fprintf(stderr, "[lom-native] Movie RGB24 pipe read failed after %u frames: %s (%d).\n", movie->frame_count, strerror(movie->read_result.error),
                movie->read_result.error);
        return LOM_NATIVE_MOVIE_ERROR;
    }

    if (movie->audio_error)
    {
        fprintf(stderr, "[lom-native] MOVIE audio output failed after %u frames.\n", movie->frame_count);
        return LOM_NATIVE_MOVIE_ERROR;
    }

    if (invalid_completion || (!movie->skipped && !lom_movie_decoder_succeeded(close_result)))
    {
        fprintf(stderr, "[lom-native] ffmpeg video decode failed: wait status=0x%X wait error=%d close error=%d.\n", close_result.wait_status,
                close_result.wait_error, close_result.stream_error);
        return LOM_NATIVE_MOVIE_ERROR;
    }

    if (!movie->skipped && movie->frame_count < movie->limit)
    {
        fprintf(stderr, "[lom-native] Movie RGB24 stream ended early after %u/%u frames.\n", movie->frame_count, movie->limit);
        return LOM_NATIVE_MOVIE_ERROR;
    }

    return movie->skipped ? LOM_NATIVE_MOVIE_SKIPPED : LOM_NATIVE_MOVIE_OK;
}

LomNativeMovieStatus lom_native_movie_play(unsigned int movie_index)
{
    MoviePlayback movie = {0};

    s_presented_frames = 0;

    if (lom_native_switch(LOM_SWITCH_SKIP_MOVIES))
    {
        return LOM_NATIVE_MOVIE_SKIPPED;
    }

    if (open_movie(&movie, movie_index) != 0)
    {
        return LOM_NATIVE_MOVIE_ERROR;
    }

    if (start_movie_audio(&movie) != 0)
    {
        return abandon_movie(&movie);
    }

    movie.video_frame = malloc(LOM_NATIVE_MOVIE_FRAME_BYTES);
    movie.video_pixels = malloc(LOM_NATIVE_MOVIE_PIXELS * sizeof(*movie.video_pixels));

    if (movie.video_frame == NULL || movie.video_pixels == NULL || show_movie(&movie) != 0)
    {
        return abandon_movie(&movie);
    }

    return finish_movie(&movie);
}
