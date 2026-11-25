#ifndef LOM_MOVIE_DECODER_H
#define LOM_MOVIE_DECODER_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

typedef struct LomMovieReadResult
{
    size_t bytes;

    int eof;

    int error;
} LomMovieReadResult;

typedef struct LomMovieCloseResult
{
    int wait_status;

    int wait_error;

    int stream_error;
} LomMovieCloseResult;

/**
 * @brief Start ffmpeg decoding a movie's video to raw RGB frames of a size.
 * @param path The video, as lom_movie_prepare_file wrote it.
 * @param frame_limit Stop after this many frames, or 0 for all of them.
 * @param width The frames' width.
 * @param height Their height.
 * @param child Set to ffmpeg's process.
 * @return The stream of frames, or NULL when ffmpeg can't be started.
 */
FILE* lom_movie_decoder_open_sized(const char* path, unsigned int frame_limit, unsigned int width, unsigned int height, pid_t* child);

/**
 * @brief lom_movie_decoder_open_sized at the PS1's 320 by 240.
 * @param path The video.
 * @param frame_limit Stop after this many frames, or 0 for all of them.
 * @param child Set to ffmpeg's process.
 * @return The stream of frames, or NULL.
 */
FILE* lom_movie_decoder_open(const char* path, unsigned int frame_limit, pid_t* child);

/**
 * @brief Read one whole frame from ffmpeg, waiting for it.
 * @param stream The stream lom_movie_decoder_open gave.
 * @param buffer Where it goes.
 * @param bytes The frame's size.
 * @return How many bytes came, and whether the stream ended or failed first.
 */
LomMovieReadResult lom_movie_decoder_read_frame(FILE* stream, void* buffer, size_t bytes);

/**
 * @brief Close ffmpeg's stream and wait for it to exit.
 * @param stream The stream.
 * @param child ffmpeg's process.
 * @param cancel Nonzero to stop it first, for a movie the player skipped.
 * @return How it exited, and any error waiting for it or closing the stream.
 */
LomMovieCloseResult lom_movie_decoder_close(FILE* stream, pid_t child, int cancel);

/**
 * @brief Check whether ffmpeg finished without an error.
 * @param result What lom_movie_decoder_close returned.
 * @return Nonzero when it did.
 */
int lom_movie_decoder_succeeded(LomMovieCloseResult result);

#endif
