#include "lom_native_remaster.h"
#include "remaster_internal.h"

#include <stdint.h>
#include <stdlib.h>

#define PATH_SIZE 1024
#define XWMA_FMT_BYTES 18
#define WMA_V2_TAG 0x0161

static const char* const s_movie_files[] = {"system/movie/opening", "system/movie/post", "system/movie/end1", "system/movie/staff", "system/movie/fin"};

static const char* const s_movie_sounds[] = {NULL, "sound/music_0013.psb", NULL, NULL, NULL};

#define MOVIE_FILES (sizeof(s_movie_files) / sizeof(s_movie_files[0]))

_Static_assert(sizeof(s_movie_sounds) == sizeof(s_movie_files), "a sound entry for each movie");

/**
 * @note The Remaster's movies are the PS1's remastered, the same length, as
 *       MP4s of their own names: OPEN.STR is opening, POST.STR post, END1.STR
 *       end1, STAFF.STR staff and FIN.STR fin.
 */
const char* lom_native_remaster_movie_file(unsigned movie)
{
    return movie < MOVIE_FILES ? s_movie_files[movie] : NULL;
}

int lom_native_remaster_movie_path(unsigned movie, char* path, unsigned long size)
{
    const char* file = lom_native_remaster_movie_file(movie);

    if (!file || !path || size == 0 || !lom_native_remaster_found() || !lom_native_remaster_hd_movies() || !lom_native_remaster_hd_movies_supported())
    {
        return 0;
    }

    if ((unsigned long)snprintf(path, size, "%s/%s", lom_native_remaster_root(), file) >= size)
    {
        return 0;
    }

    FILE* check = fopen(path, "rb");

    if (!check)
    {
        return 0;
    }

    fclose(check);
    return 1;
}

/**
 * @brief Write a RIFF chunk: its tag, its size and its bytes.
 * @param file Where.
 * @param tag The four-letter tag.
 * @param data The bytes.
 * @param size How many.
 * @return 0, or -1.
 */
static int write_chunk(FILE* file, const char tag[4], const unsigned char* data, uint32_t size)
{
    const unsigned char header[8] = {tag[0], tag[1], tag[2], tag[3], size & 0xFF, size >> 8 & 0xFF, size >> 16 & 0xFF, size >> 24};
    return fwrite(header, 1, sizeof(header), file) == sizeof(header) && fwrite(data, 1, size, file) == size ? 0 : -1;
}

/**
 * @note The track is a sound archive like the music's, but WMA (version 2)
 *       rather than PCM: its fmt, dpds and data chunks are an xWMA file's,
 *       which ffmpeg reads once they're put back in their RIFF.
 */
int lom_native_remaster_movie_sound(unsigned movie, FILE* file)
{
    const char* sound = movie < MOVIE_FILES ? s_movie_sounds[movie] : NULL;

    if (!sound)
    {
        return 0;
    }

    char path[PATH_SIZE];
    size_t size = 0;

    if (!file || (size_t)snprintf(path, sizeof(path), "%s/%s", lom_native_remaster_root(), sound) >= sizeof(path))
    {
        return -1;
    }

    unsigned char* data = lom_remaster_load(path, &size);
    LomPsb psb;

    if (!data || lom_psb_open(&psb, data, size))
    {
        free(data);
        return -1;
    }

    size_t arch, fmt_value, dpds_value, data_value;
    const unsigned char *fmt, *dpds, *pcm;
    size_t fmt_size, dpds_size, pcm_size;
    int result = -1;

    if (!lom_psb_path(&psb, psb.root, "voice/music_voice/channelList/0/archData", &arch) && !lom_psb_member(&psb, arch, "fmt", &fmt_value) &&
        !lom_psb_member(&psb, arch, "dpds", &dpds_value) && !lom_psb_member(&psb, arch, "data", &data_value) &&
        !lom_psb_chunk(&psb, fmt_value, &fmt, &fmt_size) && !lom_psb_chunk(&psb, dpds_value, &dpds, &dpds_size) &&
        !lom_psb_chunk(&psb, data_value, &pcm, &pcm_size) && fmt_size >= XWMA_FMT_BYTES && fmt_size < UINT32_MAX / 4 && dpds_size < UINT32_MAX / 4 &&
        pcm_size < UINT32_MAX / 4)
    {
        const unsigned tag = fmt[0] | fmt[1] << 8;
        const uint32_t riff_size = 4 + 8 + (uint32_t)fmt_size + 8 + (uint32_t)dpds_size + 8 + (uint32_t)pcm_size;
        const unsigned char riff[12] = {'R', 'I', 'F', 'F', riff_size & 0xFF, riff_size >> 8 & 0xFF, riff_size >> 16 & 0xFF, riff_size >> 24,
                                        'X', 'W', 'M', 'A'};

        if (tag == WMA_V2_TAG && (fmt_size & 1) == 0 && (dpds_size & 1) == 0 && fwrite(riff, 1, sizeof(riff), file) == sizeof(riff) &&
            !write_chunk(file, "fmt ", fmt, (uint32_t)fmt_size) && !write_chunk(file, "dpds", dpds, (uint32_t)dpds_size) &&
            !write_chunk(file, "data", pcm, (uint32_t)pcm_size) && fflush(file) == 0)
        {
            result = 1;
        }
    }

    lom_psb_close(&psb);
    free(data);
    return result;
}
