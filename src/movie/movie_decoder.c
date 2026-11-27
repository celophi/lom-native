#include "movie_decoder.h"
#include "movie_process.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define OUTPUT_ARGUMENT 25

FILE* lom_movie_decoder_open_sized(const char* path, unsigned int limit, unsigned int width, unsigned int height, pid_t* child)
{
    char frame_limit[16];
    char scale[48];
    char* args[] = {"ffmpeg",
                    "-nostdin",
                    "-loglevel",
                    "error",
                    "-flags",
                    "+bitexact",
                    "-idct",
                    "simple",
                    "-f",
                    "psxstr",
                    "-i",
                    (char*)path,
                    "-map",
                    "0:v:0",
                    "-an",
                    "-sws_flags",
                    "bicubic+accurate_rnd+bitexact",
                    "-vf",
                    scale,
                    "-r",
                    "15",
                    "-pix_fmt",
                    "rgb24",
                    "-f",
                    "rawvideo",
                    NULL,
                    NULL,
                    NULL,
                    NULL};

    if (width == 0 || height == 0 || width > 1024 || height > 512)
    {
        errno = EINVAL;
        return NULL;
    }

    snprintf(scale, sizeof(scale), "scale=%u:%u", width, height);

    if (limit != 0)
    {
        snprintf(frame_limit, sizeof(frame_limit), "%u", limit);
        args[OUTPUT_ARGUMENT] = "-frames:v";
        args[OUTPUT_ARGUMENT + 1] = frame_limit;
        args[OUTPUT_ARGUMENT + 2] = "-";
    }
    else
    {
        args[OUTPUT_ARGUMENT] = "-";
    }

    return lom_movie_process_start(args, child);
}

FILE* lom_movie_decoder_open_hd(const char* path, unsigned int limit, unsigned int width, unsigned int height, pid_t* child)
{
    char frame_limit[16];
    char filters[64];
    char* args[] = {"ffmpeg", "-nostdin", "-loglevel", "error", "-i",       (char*)path, "-map", "0:v:0", "-an", "-vf",
                    filters,  "-pix_fmt", "rgba",      "-f",    "rawvideo", NULL,        NULL,   NULL,    NULL};
    const int output = 15;

    if (width == 0 || height == 0 || width > LOM_MOVIE_HD_MAX_SIDE || height > LOM_MOVIE_HD_MAX_SIDE)
    {
        errno = EINVAL;
        return NULL;
    }

    snprintf(filters, sizeof(filters), "fps=%u,scale=%u:%u:flags=bicubic", LOM_MOVIE_HD_RATE, width, height);

    if (limit != 0)
    {
        snprintf(frame_limit, sizeof(frame_limit), "%u", limit);
        args[output] = "-frames:v";
        args[output + 1] = frame_limit;
        args[output + 2] = "-";
    }
    else
    {
        args[output] = "-";
    }

    return lom_movie_process_start(args, child);
}

FILE* lom_movie_decoder_open_audio(const char* path, unsigned int rate, pid_t* child)
{
    char sample_rate[16];
    char* args[] = {"ffmpeg", "-nostdin", "-loglevel", "error",     "-i", (char*)path, "-map", "0:a:0", "-vn",
                    "-ac",    "2",        "-ar",       sample_rate, "-f", "s16le",     "-",    NULL};

    snprintf(sample_rate, sizeof(sample_rate), "%u", rate);
    return lom_movie_process_start(args, child);
}

FILE* lom_movie_decoder_open(const char* path, unsigned int limit, pid_t* child)
{
    return lom_movie_decoder_open_sized(path, limit, 320, 240, child);
}

LomMovieReadResult lom_movie_decoder_read_frame(FILE* stream, void* buffer, size_t bytes)
{
    LomMovieReadResult result = {0, 0, 0};

    while (result.bytes < bytes)
    {
        errno = 0;
        result.bytes += fread((unsigned char*)buffer + result.bytes, 1, bytes - result.bytes, stream);

        if (ferror(stream))
        {
            int error = errno;

            if (error == EINTR)
            {
                clearerr(stream);
                continue;
            }

            result.error = error != 0 ? error : EIO;
            break;
        }

        if (feof(stream))
        {
            result.eof = 1;
            break;
        }
    }

    return result;
}

LomMovieCloseResult lom_movie_decoder_close(FILE* stream, pid_t child, int cancel)
{
    LomMovieCloseResult result = {0, 0, 0};

    if (cancel)
    {
        unsigned char discard[8192];
        LomMovieReadResult read_result;

        lom_movie_process_cancel(child);

        do
        {
            read_result = lom_movie_decoder_read_frame(stream, discard, sizeof(discard));
        } while (read_result.bytes == sizeof(discard) && read_result.error == 0);

        result.stream_error = read_result.error;
    }

    if (fclose(stream) != 0 && result.stream_error == 0)
    {
        result.stream_error = errno != 0 ? errno : EIO;
    }

    lom_movie_process_wait(child, &result);
    return result;
}

int lom_movie_decoder_succeeded(LomMovieCloseResult result)
{
    return result.wait_error == 0 && result.stream_error == 0 && lom_movie_process_exited_cleanly(result.wait_status);
}
