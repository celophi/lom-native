#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "movie_source.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOM_STR_SECTOR_DATA_BYTES 2048
#define LOM_STR_RAW_SECTOR_BYTES 2352
#define LOM_STR_MODE2_BYTES 2336
#define LOM_STR_SIZE1_BYTES 2340
#define LOM_STR_RIFF_HEADER_BYTES 0x2C
#define LOM_STR_ID 0x0160
#define LOM_STR_TYPE_VIDEO 0x8001
#define LOM_STR_TYPE_AUDIO_NORMAL 0x0000
#define LOM_STR_TYPE_AUDIO_END 0x0001
#define LOM_STR_VIDEO_DATA_BYTES 0x7E0
#define LOM_STR_AUDIO_AKAO_OFFSET 0x80
#define LOM_STR_AUDIO_PITCH_OFFSET 0x9C
#define LOM_STR_AUDIO_SIZE_OFFSET 0xA0
#define LOM_STR_AUDIO_DATA_OFFSET 0xD0
#define LOM_STR_AUDIO_DATA_BYTES 0x690
#define LOM_STR_AUDIO_SAMPLES_PER_SECTOR ((LOM_STR_AUDIO_DATA_BYTES / 16) * 28)
#define LOM_STR_AKAO_MAGIC 0x4F414B41u

typedef struct LomMovieSourceLayout
{
    size_t sector_bytes;

    size_t payload_offset;

    long stream_offset;
} LomMovieSourceLayout;

typedef struct LomSpuAdpcmState
{
    int previous_sample;

    int previous_previous_sample;
} LomSpuAdpcmState;

/**
 * @brief Read a little-endian 16-bit word.
 * @param p Its first byte.
 * @return The word.
 */
static uint16_t read_le16(const unsigned char* p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/**
 * @brief Read a little-endian 32-bit word.
 * @param p Its first byte.
 * @return The word.
 */
static uint32_t read_le32(const unsigned char* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Check whether bytes start with a raw CD sector's sync pattern.
 * @param p The bytes.
 * @param size How many there are.
 * @return Nonzero when they do.
 */
static int has_raw_cd_sync(const unsigned char* p, size_t size)
{
    size_t i;

    if (size < 12 || p[0] != 0 || p[11] != 0)
    {
        return 0;
    }

    for (i = 1; i < 11; i++)
    {
        if (p[i] != 0xFF)
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Check whether bytes start with the game's STR sector header.
 * @param p The bytes.
 * @param size How many there are.
 * @return Nonzero when they do.
 */
static int has_lom_str_header(const unsigned char* p, size_t size)
{
    uint16_t type;

    if (size < 32 || read_le16(p) != LOM_STR_ID)
    {
        return 0;
    }

    type = read_le16(p + 2);
    return type == LOM_STR_TYPE_VIDEO || type == LOM_STR_TYPE_AUDIO_NORMAL || type == LOM_STR_TYPE_AUDIO_END;
}

/**
 * @brief Work out how a movie file is laid out, raw CD sectors or a
 *        RIFF-wrapped CD image among them, from its first bytes.
 * @param source The file.
 * @param layout Set to the layout.
 * @return 0, or -1 when it isn't one this reads.
 */
static int detect_movie_source_layout(FILE* source, LomMovieSourceLayout* layout)
{
    unsigned char probe[LOM_STR_RAW_SECTOR_BYTES + LOM_STR_RIFF_HEADER_BYTES];
    size_t bytes_read;

    rewind(source);
    bytes_read = fread(probe, 1, sizeof(probe), source);
    rewind(source);

    if (bytes_read >= LOM_STR_RIFF_HEADER_BYTES + LOM_STR_RAW_SECTOR_BYTES && memcmp(probe, "RIFF", 4) == 0 && memcmp(probe + 8, "CDXA", 4) == 0 &&
        has_raw_cd_sync(probe + LOM_STR_RIFF_HEADER_BYTES, bytes_read - LOM_STR_RIFF_HEADER_BYTES))
    {
        layout->sector_bytes = LOM_STR_RAW_SECTOR_BYTES;
        layout->payload_offset = 24;
        layout->stream_offset = LOM_STR_RIFF_HEADER_BYTES;
        return 0;
    }

    if (bytes_read >= LOM_STR_RAW_SECTOR_BYTES && has_raw_cd_sync(probe, bytes_read) && has_lom_str_header(probe + 24, bytes_read - 24))
    {
        layout->sector_bytes = LOM_STR_RAW_SECTOR_BYTES;
        layout->payload_offset = 24;
        layout->stream_offset = 0;
        return 0;
    }

    if (bytes_read >= LOM_STR_SIZE1_BYTES && has_lom_str_header(probe + 12, bytes_read - 12))
    {
        layout->sector_bytes = LOM_STR_SIZE1_BYTES;
        layout->payload_offset = 12;
        layout->stream_offset = 0;
        return 0;
    }

    if (bytes_read >= LOM_STR_MODE2_BYTES && has_lom_str_header(probe + 8, bytes_read - 8))
    {
        layout->sector_bytes = LOM_STR_MODE2_BYTES;
        layout->payload_offset = 8;
        layout->stream_offset = 0;
        return 0;
    }

    if (bytes_read >= LOM_STR_SECTOR_DATA_BYTES && has_lom_str_header(probe, bytes_read))
    {
        layout->sector_bytes = LOM_STR_SECTOR_DATA_BYTES;
        layout->payload_offset = 0;
        layout->stream_offset = 0;
        return 0;
    }

    return -1;
}

int lom_movie_temp_file(char* path, size_t path_size, FILE** file_out)
{
    int fd;
#ifdef _WIN32
    const char* folder = getenv("TEMP");

    if (folder == NULL || *folder == '\0')
    {
        folder = getenv("TMP");
    }

    int n = folder != NULL && *folder != '\0' ? snprintf(path, path_size, "%s/lom-native-movie-XXXXXX", folder) : -1;

    if (n < 0 || (size_t)n >= path_size)
    {
        return -1;
    }
#else
    static const char template_path[] = "/tmp/lom-native-movie-XXXXXX";

    if (path_size < sizeof(template_path))
    {
        return -1;
    }

    memcpy(path, template_path, sizeof(template_path));
#endif
    fd = mkstemp(path);

    if (fd < 0)
    {
        path[0] = '\0';
        return -1;
    }

    *file_out = fdopen(fd, "w+b");

    if (*file_out == NULL)
    {
        close(fd);
        unlink(path);
        path[0] = '\0';
        return -1;
    }

    return 0;
}

/**
 * @brief Write one video sector as a raw CD sector, with its sync and header,
 *        which is how ffmpeg reads STR video.
 * @param output The temporary file.
 * @param payload The sector's video data.
 * @return 0, or -1 when the write fails.
 */
static int write_ffmpeg_video_sector(FILE* output, const unsigned char* payload)
{
    static const unsigned char sync_header[12] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    unsigned char raw_sector[LOM_STR_RAW_SECTOR_BYTES];

    memset(raw_sector, 0, sizeof(raw_sector));
    memcpy(raw_sector, sync_header, sizeof(sync_header));
    raw_sector[15] = 2;

    raw_sector[16] = 0;
    raw_sector[17] = 0;
    raw_sector[18] = 0x42;
    raw_sector[19] = 0;
    memcpy(raw_sector + 20, raw_sector + 16, 4);
    memcpy(raw_sector + 24, payload, LOM_STR_SECTOR_DATA_BYTES);

    return fwrite(raw_sector, 1, sizeof(raw_sector), output) == sizeof(raw_sector) ? 0 : -1;
}

/**
 * @brief Clamp a sample to 16 bits.
 * @param value The sample.
 * @return It, from -32768 to 32767.
 */
static short clamp_s16(int value)
{
    if (value > 32767)
    {
        return 32767;
    }

    if (value < -32768)
    {
        return -32768;
    }

    return (short)value;
}

/**
 * @brief Decode the movie's XA ADPCM blocks into 16-bit samples, carrying the
 *        filter state from block to block.
 * @param input The blocks.
 * @param input_bytes Their size.
 * @param state The channel's filter state.
 * @param output Where the samples go.
 * @param output_capacity Room for how many.
 * @return How many samples came out.
 */
static size_t decode_spu_adpcm(const unsigned char* input, size_t input_bytes, LomSpuAdpcmState* state, short* output, size_t output_capacity)
{
    static const int positive_filter[5] = {0, 60, 115, 98, 122};
    static const int negative_filter[5] = {0, 0, -52, -55, -60};
    size_t block_count;
    size_t block_index;
    size_t output_index;

    block_count = input_bytes / 16;
    output_index = 0;

    for (block_index = 0; block_index < block_count; block_index++)
    {
        const unsigned char* block;
        int shift_in;
        int sample_shift;
        int filter;
        int byte_index;

        if (output_index + 28 > output_capacity)
        {
            break;
        }

        block = input + block_index * 16;
        shift_in = block[0] & 0x0F;
        sample_shift = shift_in > 12 ? 9 : 12 - shift_in;
        filter = (block[0] >> 4) & 0x07;

        if (filter > 4)
        {
            filter = 4;
        }

        for (byte_index = 0; byte_index < 14; byte_index++)
        {
            unsigned char packed;
            int nibble_index;

            packed = block[2 + byte_index];

            for (nibble_index = 0; nibble_index < 2; nibble_index++)
            {
                int nibble;
                int sample;

                nibble = nibble_index == 0 ? (packed & 0x0F) : (packed >> 4);

                if (nibble & 8)
                {
                    nibble -= 16;
                }

                sample = nibble * (1 << sample_shift);
                sample += (state->previous_sample * positive_filter[filter]) >> 6;
                sample += (state->previous_previous_sample * negative_filter[filter]) >> 6;
                sample = clamp_s16(sample);

                output[output_index++] = (short)sample;
                state->previous_previous_sample = state->previous_sample;
                state->previous_sample = sample;
            }
        }
    }

    return output_index;
}

/**
 * @brief Write a sector's left and right samples to the PCM file, interleaved.
 * @param output The PCM file.
 * @param left The left samples.
 * @param left_samples How many.
 * @param right The right samples.
 * @param right_samples How many; it has to match.
 * @return 0, or -1.
 */
static int write_audio_frame(FILE* output, const short* left, size_t left_samples, const short* right, size_t right_samples)
{
    size_t sample_count;
    size_t i;

    if (left_samples != right_samples)
    {
        return -1;
    }

    sample_count = left_samples;

    for (i = 0; i < sample_count; i++)
    {
        short stereo[2];
        stereo[0] = left[i];
        stereo[1] = right[i];

        if (fwrite(stereo, sizeof(short), 2, output) != 2)
        {
            return -1;
        }
    }

    return 0;
}

typedef struct
{
    LomSpuAdpcmState state[2];
    short left[LOM_STR_AUDIO_SAMPLES_PER_SECTOR];
    short right[LOM_STR_AUDIO_SAMPLES_PER_SECTOR];
    size_t left_count;
    size_t right_count;
    uint32_t pending_frame;
    int have_left;
    int have_right;
} MovieAudioSplit;

/**
 * @brief Copy one video sector into the video file, noting the picture's size
 *        from the first.
 * @param video_file The video file.
 * @param payload The sector's data, with its STR header.
 * @param prepared The movie, whose size and video sector count this sets.
 * @return 0, or -1 when the header is wrong or the write fails.
 */
static int copy_video_sector(FILE* video_file, const unsigned char* payload, LomPreparedMovie* prepared)
{
    uint16_t chunk_index = read_le16(payload + 4);
    uint16_t sector_count = read_le16(payload + 6);
    uint32_t frame_size = read_le32(payload + 12);

    if (sector_count == 0 || chunk_index >= sector_count || frame_size > (uint32_t)sector_count * LOM_STR_VIDEO_DATA_BYTES)
    {
        fprintf(stderr, "[lom-native] Invalid LoM STR video sector header.\n");
        return -1;
    }

    if (write_ffmpeg_video_sector(video_file, payload) != 0)
    {
        return -1;
    }

    if (prepared->video_sector_count == 0)
    {
        prepared->width = read_le16(payload + 16);
        prepared->height = read_le16(payload + 18);
    }

    prepared->video_sector_count++;
    return 0;
}

/**
 * @brief Decode one sound sector, a channel of one AKAO frame, and write the
 *        frame as stereo PCM once both channels are in.
 * @param audio The frame so far.
 * @param audio_file The sound file.
 * @param payload The sector's data, with its STR header.
 * @param prepared The movie, whose sound frame count this adds to.
 * @return 0, 1 when the sector isn't movie sound and is skipped, or -1 when the
 *         sound can't be read or written.
 */
static int split_audio_sector(MovieAudioSplit* audio, FILE* audio_file, const unsigned char* payload, LomPreparedMovie* prepared)
{
    uint16_t chunk_index = read_le16(payload + 4);
    uint16_t sector_count = read_le16(payload + 6);
    uint32_t frame_number = read_le32(payload + 8);
    uint32_t akao_magic = read_le32(payload + LOM_STR_AUDIO_AKAO_OFFSET);
    uint32_t pitch = read_le32(payload + LOM_STR_AUDIO_PITCH_OFFSET);
    uint32_t audio_bytes = read_le32(payload + LOM_STR_AUDIO_SIZE_OFFSET);

    if (sector_count != 2 || chunk_index >= 2 || akao_magic != LOM_STR_AKAO_MAGIC || audio_bytes == 0 || audio_bytes > LOM_STR_AUDIO_DATA_BYTES ||
        (audio_bytes & 15) != 0)
    {
        return 1;
    }

    if (pitch != 0x1000)
    {
        fprintf(stderr, "[lom-native] Unsupported LoM movie AKAO pitch 0x%X in frame %u (expected 0x1000 / 44100 Hz).\n", pitch, frame_number);
        return -1;
    }

    if ((audio->have_left || audio->have_right) && frame_number != audio->pending_frame)
    {
        fprintf(stderr, "[lom-native] Incomplete LoM movie audio frame %u before frame %u.\n", audio->pending_frame, frame_number);
        return -1;
    }

    audio->pending_frame = frame_number;

    if (chunk_index == 0)
    {
        audio->left_count = decode_spu_adpcm(payload + LOM_STR_AUDIO_DATA_OFFSET, audio_bytes, &audio->state[0], audio->left, LOM_STR_AUDIO_SAMPLES_PER_SECTOR);
        audio->have_left = 1;
    }
    else
    {
        audio->right_count =
            decode_spu_adpcm(payload + LOM_STR_AUDIO_DATA_OFFSET, audio_bytes, &audio->state[1], audio->right, LOM_STR_AUDIO_SAMPLES_PER_SECTOR);
        audio->have_right = 1;
    }

    if (audio->have_left && audio->have_right)
    {
        if (write_audio_frame(audio_file, audio->left, audio->left_count, audio->right, audio->right_count) != 0)
        {
            return -1;
        }

        prepared->audio_frame_count++;
        audio->have_left = 0;
        audio->have_right = 0;
    }

    return 0;
}

/**
 * @brief Read a movie's sectors from the start of its stream, sending the video
 *        to one file and the sound, decoded, to the other.
 * @param source The movie.
 * @param layout Its sector layout.
 * @param video_file The video file.
 * @param audio_file The sound file.
 * @param audio The sound frame in progress.
 * @param prepared The movie's counts, which this fills in.
 * @param ignored Counts the sectors that aren't movie data.
 * @return 0, or -1 when the movie can't be read or split.
 */
static int split_movie(FILE* source, const LomMovieSourceLayout* layout, FILE* video_file, FILE* audio_file, MovieAudioSplit* audio, LomPreparedMovie* prepared,
                       unsigned int* ignored)
{
    unsigned char source_sector[LOM_STR_RAW_SECTOR_BYTES];

    if (fseek(source, layout->stream_offset, SEEK_SET) != 0)
    {
        return -1;
    }

    for (;;)
    {
        size_t bytes_read = fread(source_sector, 1, layout->sector_bytes, source);

        if (bytes_read == 0)
        {
            return 0;
        }

        if (bytes_read != layout->sector_bytes)
        {
            fprintf(stderr, "[lom-native] Movie ends with a partial source sector (%zu/%zu bytes).\n", bytes_read, layout->sector_bytes);
            return -1;
        }

        const unsigned char* payload = source_sector + layout->payload_offset;

        if (!has_lom_str_header(payload, LOM_STR_SECTOR_DATA_BYTES))
        {
            (*ignored)++;
            continue;
        }

        if (read_le16(payload + 2) == LOM_STR_TYPE_VIDEO)
        {
            if (copy_video_sector(video_file, payload, prepared) != 0)
            {
                return -1;
            }

            continue;
        }

        int result = split_audio_sector(audio, audio_file, payload, prepared);

        if (result < 0)
        {
            return -1;
        }

        if (result > 0)
        {
            (*ignored)++;
        }
    }
}

/**
 * @brief Delete the video file and drop its path, after a failure.
 * @param prepared The movie.
 */
static void discard_video_file(LomPreparedMovie* prepared)
{
    unlink(prepared->video_path);
    prepared->video_path[0] = '\0';
}

int lom_movie_prepare_file(const char* movie_path, LomPreparedMovie* prepared)
{
    MovieAudioSplit audio = {0};
    LomMovieSourceLayout layout;
    FILE* source;
    FILE* video_file;
    FILE* audio_file;
    unsigned int ignored_sector_count = 0;

    memset(prepared, 0, sizeof(*prepared));
    source = fopen(movie_path, "rb");

    if (source == NULL)
    {
        fprintf(stderr, "[lom-native] Could not open movie %s.\n", movie_path);
        return -1;
    }

    if (detect_movie_source_layout(source, &layout) != 0)
    {
        fprintf(stderr, "[lom-native] %s is not a recognized LoM STR sector dump (expected 2048/2336/2340/2352-byte sectors).\n", movie_path);
        fclose(source);
        return -1;
    }

    if (lom_movie_temp_file(prepared->video_path, sizeof(prepared->video_path), &video_file) != 0)
    {
        fprintf(stderr, "[lom-native] Could not create temporary STR video stream.\n");
        fclose(source);
        return -1;
    }

    audio_file = tmpfile();

    if (audio_file == NULL)
    {
        fprintf(stderr, "[lom-native] Could not create temporary movie audio stream.\n");
        fclose(video_file);
        discard_video_file(prepared);
        fclose(source);
        return -1;
    }

    if (split_movie(source, &layout, video_file, audio_file, &audio, prepared, &ignored_sector_count) != 0)
    {
        fclose(audio_file);
        fclose(video_file);
        discard_video_file(prepared);
        fclose(source);
        return -1;
    }

    fclose(source);

    if (fclose(video_file) != 0)
    {
        fclose(audio_file);
        discard_video_file(prepared);
        return -1;
    }

    if (audio.have_left || audio.have_right)
    {
        fprintf(stderr, "[lom-native] Movie ends with an incomplete AKAO stereo frame.\n");
        fclose(audio_file);
        discard_video_file(prepared);
        return -1;
    }

    if (prepared->video_sector_count == 0)
    {
        fprintf(stderr, "[lom-native] Movie contains no MDEC video sectors.\n");
        fclose(audio_file);
        discard_video_file(prepared);
        return -1;
    }

    if (prepared->audio_frame_count != 0)
    {
        rewind(audio_file);
        prepared->audio_pcm = audio_file;
    }
    else
    {
        fclose(audio_file);
        prepared->audio_pcm = NULL;
    }

    fprintf(stderr, "[lom-native] Prepared LoM STR: %zu-byte source sectors, %u video sectors, %u AKAO stereo frames%s.\n", layout.sector_bytes,
            prepared->video_sector_count, prepared->audio_frame_count, ignored_sector_count != 0 ? " (ignored non-movie sectors)" : "");
    return 0;
}

void lom_movie_cleanup_source(LomPreparedMovie* prepared)
{
    if (prepared->audio_pcm != NULL)
    {
        fclose(prepared->audio_pcm);
        prepared->audio_pcm = NULL;
    }

    if (prepared->video_path[0] != '\0')
    {
        unlink(prepared->video_path);
        prepared->video_path[0] = '\0';
    }
}
