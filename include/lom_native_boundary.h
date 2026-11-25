#ifndef LOM_NATIVE_BOUNDARY_H
#define LOM_NATIVE_BOUNDARY_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_BOUNDARY_FILES = 0,
        LOM_BOUNDARY_SOUND = 1,
        LOM_BOUNDARY_INPUT = 2,
        LOM_BOUNDARY_SAVES = 3,
        LOM_BOUNDARY_OVERLAYS = 4,
        LOM_BOUNDARY_MOVIES = 5,
        LOM_BOUNDARY_GRAPHICS = 6,
        LOM_BOUNDARY_COUNT = 7
    } LomBoundary;

    typedef enum
    {
        LOM_BOUNDARY_FILE_READ = 1,
        LOM_BOUNDARY_FILE_STREAM = 2,
        LOM_BOUNDARY_FILE_CHUNKED = 3,
        LOM_BOUNDARY_FILE_CALLBACK_READ = 4,
        LOM_BOUNDARY_FILE_SEEK = 5,
        LOM_BOUNDARY_FILE_SIZE = 6
    } LomBoundaryFileCall;

    typedef enum
    {
        LOM_BOUNDARY_SAVE_OPEN = 1,
        LOM_BOUNDARY_SAVE_CLOSE,
        LOM_BOUNDARY_SAVE_READ,
        LOM_BOUNDARY_SAVE_WRITE,
        LOM_BOUNDARY_SAVE_ERASE,
        LOM_BOUNDARY_SAVE_RENAME,
        LOM_BOUNDARY_SAVE_CARD_INFO,
        LOM_BOUNDARY_SAVE_CARD_LOAD,
        LOM_BOUNDARY_SAVE_CARD_CLEAR,
        LOM_BOUNDARY_SAVE_FORMAT,
        LOM_BOUNDARY_SAVE_READ_SECTOR,
        LOM_BOUNDARY_SAVE_WRITE_SECTOR
    } LomBoundarySaveCall;

    typedef void (*LomBoundaryObserver)(LomBoundary boundary, unsigned what, unsigned a, unsigned b);

    /**
     * @brief Report one call across a boundary.
     * @param boundary Which boundary.
     * @param what Which call, as that boundary numbers them.
     * @param a First detail, as that boundary defines it.
     * @param b Second detail, as that boundary defines it.
     */
    void lom_native_boundary_event(LomBoundary boundary, unsigned what, unsigned a, unsigned b);

    /** @brief Watch every boundary event, or stop with NULL. Tests use it. */
    void lom_native_boundary_set_observer(LomBoundaryObserver observer);

    /** @brief The boundary's name, for logs: "files", "sound" and so on. */
    const char* lom_native_boundary_name(LomBoundary boundary);

#ifdef __cplusplus
}
#endif
#endif
