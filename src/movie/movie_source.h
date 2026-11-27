#ifndef LOM_MOVIE_SOURCE_H
#define LOM_MOVIE_SOURCE_H

#include <stdio.h>

#define LOM_MOVIE_PATH_MAX 4096

typedef struct LomPreparedMovie
{
    char video_path[LOM_MOVIE_PATH_MAX];

    FILE* audio_pcm;

    unsigned int video_sector_count;

    unsigned int audio_frame_count;

    unsigned int width;

    unsigned int height;
} LomPreparedMovie;

/**
 * @brief Split one of the game's STR movies into what ffmpeg reads: a temporary
 *        file of its video sectors, in the raw CD layout ffmpeg knows, and its
 *        XA audio decoded to 16-bit stereo PCM.
 * @param path The movie on the extracted disc.
 * @param prepared Filled in; lom_movie_cleanup_source frees it.
 * @return 0, or -1 when the file isn't a movie this reads.
 */
int lom_movie_prepare_file(const char* path, LomPreparedMovie* prepared);

/**
 * @brief Make a temporary file, readable only by the player, for ffmpeg to
 *        read: the video sectors of a disc movie, or a Remaster movie's sound.
 * @param path Set to its path, to delete when done.
 * @param path_size Bytes at @p path.
 * @param file_out Set to the file, open for writing and reading.
 * @return 0, or -1.
 */
int lom_movie_temp_file(char* path, size_t path_size, FILE** file_out);

/**
 * @brief Close and delete what lom_movie_prepare_file made.
 * @param prepared It.
 */
void lom_movie_cleanup_source(LomPreparedMovie* prepared);

#endif
