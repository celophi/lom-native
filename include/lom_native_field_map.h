#ifndef LOM_NATIVE_FIELD_MAP_H
#define LOM_NATIVE_FIELD_MAP_H

extern unsigned char lom_native_field_map_header[];

/**
 * @brief Decode the map file just read into the load buffer.
 * @param file The load buffer, where the file stays.
 *
 * Anything that doesn't fit the map format stops the program, saying where.
 */
void lom_native_field_decode_map(const void* file);

/**
 * @brief Count the records decoded from the room's map, for the tests.
 * @return The count.
 */
unsigned lom_native_field_map_record_count(void);

/**
 * @brief Measure how much of the decoded map's memory is in use, for the tests.
 * @return The bytes.
 */
unsigned long lom_native_field_map_arena_bytes(void);

#endif
