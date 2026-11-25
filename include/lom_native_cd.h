#ifndef LOM_NATIVE_CD_H
#define LOM_NATIVE_CD_H

#include <stddef.h>
#include <stdint.h>

#if defined(VERSION_JP)
#define LOM_NATIVE_DISC_EXECUTABLE "SLPS_021.70"
#define LOM_NATIVE_DISC_SERIAL "SLPS-02170"
#else
#define LOM_NATIVE_DISC_EXECUTABLE "SLUS_010.13"
#define LOM_NATIVE_DISC_SERIAL "SLUS-01013"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum LomNativeCdError
    {
        LOM_NATIVE_CD_ERROR_NONE = 0,
        LOM_NATIVE_CD_ERROR_INVALID_RESOURCE,
        LOM_NATIVE_CD_ERROR_NOT_FOUND,
        LOM_NATIVE_CD_ERROR_IO,
        LOM_NATIVE_CD_ERROR_UNSUPPORTED,
        LOM_NATIVE_CD_ERROR_INVALID_DESTINATION,
        LOM_NATIVE_CD_ERROR_DECOMPRESSION,
        LOM_NATIVE_CD_ERROR_QUEUE_FULL,
        LOM_NATIVE_CD_ERROR_CALLBACK_BLOCKED,
    } LomNativeCdError;

    /**
     * @brief Override the root directory containing the user's extracted game
     *        files.
     * @param path Directory containing the executable
     *        (LOM_NATIVE_DISC_EXECUTABLE), BIN/, SOUND/, and other disc
     *        content.
     * @return Zero on success, or -1 when the path is invalid or too long.
     */
    int lom_native_cd_set_disc_root(const char* path);

    /**
     * @brief Return the currently configured extracted-disc root.
     * @return NUL-terminated host path owned by the native CD backend.
     */
    const char* lom_native_cd_get_disc_root(void);

    /**
     * @brief Resolve a known LoM resource index to a host file path.
     * @param resource_index LoM CD_RES_* resource index.
     * @param path Destination buffer for the host path.
     * @param path_size Size of @p path in bytes.
     * @return Zero on success, or -1 when the resource is not mapped or the
     *        buffer is too small.
     */
    int lom_native_cd_resolve_resource_path(int resource_index, char* path, size_t path_size);

    /**
     * @brief Return the most recent native resource-backend error.
     * @return LomNativeCdError value.
     */
    LomNativeCdError lom_native_cd_get_last_error(void);

    typedef unsigned char* (*LomNativeCdCallback)(int32_t done, uint32_t remaining);

    /**
     * @brief Check whether a read with a sector callback is still queued.
     * @param resource The file's resource index.
     * @param callback The callback it was queued with.
     * @return 1 if it is, otherwise 0.
     */
    int lom_native_cd_callback_pending(int resource, LomNativeCdCallback callback);

    /**
     * @brief Drop the queued reads of a file with a sector callback, closing
     *        their files.
     * @param resource The file's resource index.
     * @param callback The callback they were queued with.
     * @return How many were dropped.
     */
    unsigned lom_native_cd_cancel_callback(int resource, LomNativeCdCallback callback);

    /** Only changes when init, reset or stop throws away everything in flight. */
    unsigned lom_native_cd_reset_epoch(void);

    typedef enum LomNativeCdCall
    {
        LOM_NATIVE_CD_CALL_READ,
        LOM_NATIVE_CD_CALL_STREAM,
        LOM_NATIVE_CD_CALL_WAIT,
    } LomNativeCdCall;

    typedef struct LomNativeCdHooks
    {
        void (*before)(LomNativeCdCall call, int32_t resource);
        void (*after)(LomNativeCdCall call, int32_t resource, int32_t result);
    } LomNativeCdHooks;

    /** @brief Hooks for the game's reads and waits from now on, or none with NULL. */
    void lom_native_cd_set_hooks(const LomNativeCdHooks* hooks);

    typedef int32_t (*LomNativeCdReplacement)(int32_t resource, unsigned char* data, int32_t size, size_t room);

    /** @brief Set the replacement for streamed files, or none with NULL. */
    void lom_native_cd_set_replacement(LomNativeCdReplacement replacement);

    /** @brief The disc path of a resource (like "MAP/MAP0/BON_1F00.PRS"), or NULL. */
    const char* lom_native_cd_resource_name(int resource_index);

    /** The port's own loads, without hooks or boundary events. */
    int32_t lom_native_cd_stream(int32_t resource, unsigned char* destination);

    /**
     * @brief Queue a read of a whole file into memory, as the game's
     *        cdrom_queue_read does, through the port's file hooks.
     * @param resource_index The file's resource index.
     * @param dst_buffer Where it goes.
     * @return What the game's queue returns.
     */
    int32_t lom_native_cd_queue_read(int32_t resource, void* destination);

    /**
     * @brief Finish every queued read that has a sector callback, so their
     *        memory can be reused.
     */
    void lom_native_cd_wait(void);

#ifdef __cplusplus
}
#endif

#endif
