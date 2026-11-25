#ifndef MOVIE_PROCESS_H
#define MOVIE_PROCESS_H

#include "movie_decoder.h"

#include <stdio.h>

/**
 * @brief Start ffmpeg with its output piped back to the game.
 * @param args Its arguments, NULL-ended, starting with "ffmpeg".
 * @param child Set to its id.
 * @return The read end of its output, or NULL with errno set.
 */
FILE* lom_movie_process_start(char* const args[], pid_t* child);

/**
 * @brief Stop a started ffmpeg early.
 * @param child Its id.
 */
void lom_movie_process_cancel(pid_t child);

/**
 * @brief Wait for a started ffmpeg to end, after its output is closed.
 * @param child Its id.
 * @param result Its wait_status and wait_error are set.
 */
void lom_movie_process_wait(pid_t child, LomMovieCloseResult* result);

/**
 * @brief Check whether an ended ffmpeg exited cleanly.
 * @param wait_status Its status, from lom_movie_process_wait.
 * @return Nonzero when it did.
 */
int lom_movie_process_exited_cleanly(int wait_status);

#endif
