#ifndef ROUTE_RECORD_H
#define ROUTE_RECORD_H

typedef enum
{
    ROUTE_COVER_INSTRUCTION = 1,
    ROUTE_COVER_INSTRUCTION_END = 2,
    ROUTE_COVER_SCENE = 3,
    ROUTE_COVER_EXIT = 4,
    ROUTE_COVER_SONG = 5,
    ROUTE_COVER_MONSTER = 6,
} RouteCoverKind;

/**
 * @brief Whether FIELD is running: its runtime is set up and no overlay has been loaded over it.
 * @return Non-zero while FIELD's records are FIELD's.
 */
int route_field_running(void);

/**
 * @brief Start recording a route into a folder: frames.txt for each frame's
 *        fingerprints and boundary.txt for the boundary counts, with a GPU dump
 *        of one frame when LOM_ROUTE_GPU_DUMP asks for it.
 * @param directory The folder, which must exist.
 * @return 0, or -1 when frames.txt can't be opened.
 */
int route_record_open(const char* directory);

/**
 * @brief Write one FIELD frame's line: the picture and sound since the last
 *        frame, the save, the scene and the hero's position.
 * @param frame FIELD frame number.
 * @param scene Current scene.
 * @param hero_x Hero x in map pixels.
 * @param hero_z Hero z in map pixels.
 */
void route_record_frame(unsigned frame, int scene, int hero_x, int hero_z);

/**
 * @brief Write a checkpoint line.
 * @param name The checkpoint's name from the route.
 * @param frame FIELD frame number.
 * @param scene Current scene.
 */
void route_record_checkpoint(const char* name, unsigned frame, int scene);

/**
 * @brief For screens of their own (the world map, the ring menu, the
 *        encyclopedia), where FIELD's frames stop: write the fingerprint of
 *        what was drawn since the last vblank, if anything was, and with
 *        LOM_ROUTE_CAPTURE_EVERY set, save the picture on screen as
 *        screen-<vblank>.ppm every that many vblanks.
 * @param vblank The run's vblank count.
 */
void route_record_screen(unsigned vblank);

/**
 * @brief Write a line about a step: started, done or failed, with a reason.
 * @param text The line, without a newline.
 */
void route_record_note(const char* text);

/**
 * @brief Add a coverage item, once.
 * @param kind What it is.
 * @param a First value (for most kinds, the scene).
 * @param b Second value.
 * @param c Third value.
 */
void route_record_cover(RouteCoverKind kind, unsigned a, unsigned b, unsigned c);

/**
 * @brief Write the end line and the coverage file, and close the record.
 * @param steps_done Steps finished.
 * @param steps_total Steps in the plan.
 * @param failed Non-zero when a step failed.
 */
void route_record_close(int steps_done, int steps_total, int failed);

#endif
