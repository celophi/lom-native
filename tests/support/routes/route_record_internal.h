#ifndef ROUTE_RECORD_INTERNAL_H
#define ROUTE_RECORD_INTERNAL_H

#include <stdio.h>

#define ROUTE_FNV_OFFSET 1469598103934665603ull
#define ROUTE_FNV_PRIME 1099511628211ull

enum
{
    ROUTE_RECORD_PATH_BYTES = 600,
};

/**
 * @brief Mix bytes into an FNV-1a hash.
 * @param hash Hash to update.
 * @param data Bytes to mix in.
 * @param size Number of bytes.
 */
void route_record_mix(unsigned long long* hash, const void* data, unsigned long size);

/**
 * @brief Hash a block of memory.
 * @param data Bytes to hash.
 * @param size Number of bytes.
 * @return The FNV-1a hash.
 */
unsigned long long route_record_hash(const void* data, unsigned long size);

/**
 * @brief The folder the route is being recorded into.
 * @return The folder, or "" before route_record_open.
 */
const char* route_record_directory(void);

/**
 * @brief Whether a record is open.
 * @return Non-zero between route_record_open and route_record_close.
 */
int route_record_recording(void);

/**
 * @brief Read LOM_ROUTE_GPU_DUMP and start capturing the first frame's GPU
 *        commands when it asks for a dump.
 */
void route_record_gpu_open(void);

/**
 * @brief The picture fingerprint of the frame so far.
 * @return Its hash.
 */
unsigned long long route_record_picture(void);

/**
 * @brief The sound fingerprint of the frame so far.
 * @return Its hash.
 */
unsigned long long route_record_sound(void);

/**
 * @brief Start a new frame's picture and sound fingerprints, and forget the
 *        last frame's VRAM reads.
 */
void route_record_gpu_frame_start(void);

/**
 * @brief Finish a frame's GPU dump, saving it when it's the one asked for, and
 *        start the next frame's.
 * @param frame The frame's number.
 */
void route_record_gpu_frame_end(unsigned frame);

/**
 * @brief Write the coverage set, one item per line.
 * @param file Open coverage file.
 */
void route_record_write_cover(FILE* file);

/**
 * @brief How many coverage items didn't fit in the set.
 * @return The count.
 */
unsigned route_record_cover_dropped(void);

#endif
