#define _GNU_SOURCE
#include "../../src/remaster/remaster_internal.h"
#include "../../src/gpu/hd_movie.h"
#include "lom_native_remaster.h"
#include "../support/platform/remaster_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../support/lom_check.h"
#include "../support/platform/lom_test_posix.h"
#ifdef _WIN32
#include <direct.h>
#endif

#define PS1_MOVIES 5
#define PATH_SIZE 1024
#define POST_MOVIE 1
#define XWMA_FMT_BYTES 18

/**
 * @brief A renderer that can show the Remaster's movies.
 * @param width Unused.
 * @param height Unused.
 * @return 0.
 */
int lom_hd_movie_begin(unsigned width, unsigned height)
{
    (void)width;
    (void)height;
    return 0;
}

/** @brief Unused. @param rgba Unused. */
void lom_hd_movie_frame(const void* rgba)
{
    (void)rgba;
}

/** @brief Unused. */
void lom_hd_movie_end(void)
{
}

/**
 * @brief Make a folder.
 * @param path The folder.
 * @return 0, or -1.
 */
static int make_folder(const char* path)
{
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0700);
#endif
}

/**
 * @brief Check where the movie goes in a window.
 * @param window_w The window's width.
 * @param window_h Its height.
 * @param wide Nonzero when the frame is wide.
 * @param x The movie's expected left.
 * @param y Its top.
 * @param w Its width.
 * @param h Its height.
 */
static void fits(int window_w, int window_h, int wide, int x, int y, int w, int h)
{
    int rect[4] = {-1, -1, -1, -1};
    lom_hd_movie_fit(window_w, window_h, wide, rect);
    CHECK(rect[0] == x && rect[1] == y && rect[2] == w && rect[3] == h);
}

/**
 * @brief Check the MOVIE screen's five movies map to the Remaster's files of
 *        the same names, and nothing past them does.
 */
static void movies_map_to_the_remasters_files(void)
{
    static const char* const expected[PS1_MOVIES] = {"system/movie/opening", "system/movie/post", "system/movie/end1", "system/movie/staff",
                                                     "system/movie/fin"};

    for (unsigned i = 0; i < PS1_MOVIES; ++i)
    {
        CHECK(lom_native_remaster_movie_file(i) && !strcmp(lom_native_remaster_movie_file(i), expected[i]));
    }

    CHECK(lom_native_remaster_movie_file(PS1_MOVIES) == NULL);
    CHECK(lom_native_remaster_movie_file(~0u) == NULL);
}

/**
 * @brief Check a movie fills a wide frame, and in a 4:3 one is letterboxed
 *        inside the picture, whatever shape the window is.
 */
static void movies_fill_a_wide_frame_and_letterbox_in_four_three(void)
{
    fits(1920, 1080, 1, 0, 0, 1920, 1080);
    fits(1920, 1080, 0, 240, 135, 1440, 810);
    fits(1024, 768, 0, 0, 96, 1024, 576);
    fits(1024, 768, 1, 0, 96, 1024, 576);
    fits(2560, 1080, 1, 320, 0, 1920, 1080);
    fits(0, 0, 1, 0, 0, 0, 0);
    fits(-5, -5, 0, 0, 0, 0, 0);
}

/**
 * @brief Check a movie's path is given only with the Remaster found, HD
 *        movies on and the file there, and not for a movie it doesn't have.
 */
static void a_movie_path_needs_the_setting_and_the_file(void)
{
    char root[PATH_SIZE];
    char folder[PATH_SIZE], file[PATH_SIZE], path[PATH_SIZE];

    CHECK(lom_test_make_temp_folder(root, sizeof(root), "lom-remaster-movies") != NULL);
    CHECK(lom_native_remaster_hd_movies_supported());
    CHECK(lom_native_remaster_movie_path(0, path, sizeof(path)) == 0);

    lom_remaster_set_found(root);
    CHECK(lom_native_remaster_movie_path(0, path, sizeof(path)) == 0);

    snprintf(folder, sizeof(folder), "%s/system", root);
    CHECK(make_folder(folder) == 0);
    snprintf(folder, sizeof(folder), "%s/system/movie", root);
    CHECK(make_folder(folder) == 0);
    snprintf(file, sizeof(file), "%s/system/movie/opening", root);
    FILE* movie = fopen(file, "wb");
    CHECK(movie != NULL);
    fclose(movie);

    CHECK(lom_native_remaster_hd_movies());
    CHECK(lom_native_remaster_movie_path(0, path, sizeof(path)) == 1 && !strcmp(path, file));
    CHECK(lom_native_remaster_movie_path(1, path, sizeof(path)) == 0);
    CHECK(lom_native_remaster_movie_path(PS1_MOVIES, path, sizeof(path)) == 0);
    CHECK(lom_native_remaster_movie_path(0, path, 8) == 0);
    CHECK(lom_native_remaster_movie_path(0, NULL, sizeof(path)) == 0);

    lom_native_remaster_set_hd_movies(0);
    CHECK(!lom_native_remaster_hd_movies() && lom_native_remaster_movie_path(0, path, sizeof(path)) == 0);
    lom_native_remaster_set_hd_movies(1);
    CHECK(lom_native_remaster_movie_path(0, path, sizeof(path)) == 1);

    lom_remaster_set_found(NULL);
    CHECK(lom_native_remaster_movie_path(0, path, sizeof(path)) == 0);

    remove(file);
    rmdir(folder);
    snprintf(folder, sizeof(folder), "%s/system", root);
    rmdir(folder);
    rmdir(root);
}

/**
 * @brief Read a little-endian 32-bit number.
 * @param p Its bytes.
 * @return It.
 */
static unsigned read_le32(const unsigned char* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}

/**
 * @brief Put a fixture in the Remaster's folder as the post movie's track.
 * @param path Its path.
 * @param data The fixture.
 * @param size Its size.
 */
static void write_track(const char* path, const unsigned char* data, size_t size)
{
    FILE* file = fopen(path, "wb");
    CHECK(file != NULL && fwrite(data, 1, size, file) == size);
    fclose(file);
}

/**
 * @brief Check the post movie's sound is written from its own track as an
 *        xWMA file, with the track's fmt, dpds and data chunks in order, that
 *        the other movies keep their own sound, and that a missing track or one
 *        that isn't WMA is refused.
 */
static void the_post_movies_sound_comes_from_its_track(void)
{
    char root[PATH_SIZE], folder[PATH_SIZE], track[PATH_SIZE];
    unsigned char out[4096];

    CHECK(lom_test_make_temp_folder(root, sizeof(root), "lom-remaster-sound") != NULL);
    lom_remaster_set_found(root);
    snprintf(folder, sizeof(folder), "%s/sound", root);
    CHECK(make_folder(folder) == 0);
    snprintf(track, sizeof(track), "%s/sound/music_0013.psb", root);

    FILE* file = tmpfile();
    CHECK(file != NULL);
    CHECK(lom_native_remaster_movie_sound(0, file) == 0);
    CHECK(lom_native_remaster_movie_sound(PS1_MOVIES, file) == 0);
    CHECK(ftell(file) == 0);
    CHECK(lom_native_remaster_movie_sound(POST_MOVIE, file) == -1);

    write_track(track, fixture_xwma_psb, sizeof(fixture_xwma_psb));
    CHECK(lom_native_remaster_movie_sound(POST_MOVIE, NULL) == -1);
    CHECK(lom_native_remaster_movie_sound(POST_MOVIE, file) == 1);
    rewind(file);
    const size_t size = fread(out, 1, sizeof(out), file);
    fclose(file);

    const unsigned char* fmt = out + 20;
    const unsigned char* dpds = fmt + XWMA_FMT_BYTES + 8;
    const unsigned char* data = dpds + read_le32(dpds - 4) + 8;
    CHECK(size > 12 && !memcmp(out, "RIFF", 4) && read_le32(out + 4) == size - 8 && !memcmp(out + 8, "XWMA", 4));
    CHECK(!memcmp(out + 12, "fmt ", 4) && read_le32(out + 16) == XWMA_FMT_BYTES && fmt[0] == 0x61 && fmt[1] == 0x01 && fmt[2] == 2);
    CHECK(!memcmp(dpds - 8, "dpds", 4) && read_le32(dpds - 4) == 8 && read_le32(dpds) == 4096 && read_le32(dpds + 4) == 8192);
    CHECK(!memcmp(data - 8, "data", 4) && read_le32(data - 4) == 510 && data + 510 == out + size && data[0] == 0 && data[254] == 254 && data[255] == 0);

    write_track(track, fixture_psb, sizeof(fixture_psb));
    file = tmpfile();
    CHECK(file != NULL);
    CHECK(lom_native_remaster_movie_sound(POST_MOVIE, file) == -1);
    fclose(file);

    lom_remaster_set_found(NULL);
    remove(track);
    rmdir(folder);
    rmdir(root);
}

int main(void)
{
    movies_map_to_the_remasters_files();
    movies_fill_a_wide_frame_and_letterbox_in_four_three();
    a_movie_path_needs_the_setting_and_the_file();
    the_post_movies_sound_comes_from_its_track();
    puts("remaster-movies: the Remaster's movies map, fit, get their sound and are only used when they should be");
    return 0;
}
