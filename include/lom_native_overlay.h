#ifndef LOM_NATIVE_OVERLAY_H
#define LOM_NATIVE_OVERLAY_H

#include "common.h"
#include "lom_native_overlay_entry.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum LomNativeOverlayKind
    {
        LOM_NATIVE_OVERLAY_UNKNOWN = 0,
        LOM_NATIVE_OVERLAY_CHECKPS,
        LOM_NATIVE_OVERLAY_TITLE,
        LOM_NATIVE_OVERLAY_GNAME,
        LOM_NATIVE_OVERLAY_WSEL,
    } LomNativeOverlayKind;

    typedef enum LomNativeOverlayDispatchStatus
    {
        LOM_NATIVE_OVERLAY_DISPATCH_OK = 0,
        LOM_NATIVE_OVERLAY_DISPATCH_INVALID_IMAGE = -1,
        LOM_NATIVE_OVERLAY_DISPATCH_UNSUPPORTED = -2,
    } LomNativeOverlayDispatchStatus;

    typedef s32 (*LomNativeWselEntry)(intptr_t render_context);

    typedef s32 (*LomNativeTitleEntry)(intptr_t menu_context_address);

    typedef s32 (*LomNativeGnameEntry)(intptr_t render_buffers, intptr_t initial_name, intptr_t active_name, s32 source_mode, s32 history_index,
                                       intptr_t custom_name, s32 allow_empty_cancel);

    typedef struct LomNativeOverlayInfo
    {
        LomNativeOverlayKind kind;

        s32 resource_id;

        s32 overlay_id;

        const char* name;
    } LomNativeOverlayInfo;

    /**
     * @brief Identify a decompressed overlay image using its resource and
     *        header id.
     * @param resource_id LoM CD_RES_* id used to load the overlay.
     * @param image Decompressed overlay bytes in mapped PS1 RAM.
     * @param image_size Number of decompressed bytes available at @p image.
     * @param info Receives the overlay's descriptor.
     * @return Zero when the resource/header pair is known, otherwise nonzero.
     */
    int lom_native_overlay_identify(s32 resource_id, const void* image, size_t image_size, LomNativeOverlayInfo* info);

    /**
     * @brief Register the WSEL entry point the overlay dispatcher calls.
     * @param entry The entry point.
     */
    void lom_native_overlay_set_wsel_entry(LomNativeWselEntry entry);

    /**
     * @brief Register the TITLE entry point the overlay dispatcher calls.
     * @param entry Wrapper around the decomp's run_title(), or NULL to turn
     *        TITLE off.
     */
    void lom_native_overlay_set_title_entry(LomNativeTitleEntry entry);

    /**
     * @brief Register the GNAME entry point the overlay dispatcher calls.
     * @param entry Wrapper around the decomp's gname_run(), or NULL to turn
     *        GNAME off.
     */
    void lom_native_overlay_set_gname_entry(LomNativeGnameEntry entry);

    /**
     * @brief Run an identified overlay's entry point.
     * @param info Descriptor returned by lom_native_overlay_identify().
     * @param arg0 The overlay's first argument, widened to pointer size.
     * @param result Receives the overlay's game-state/result value on success.
     * @return LomNativeOverlayDispatchStatus.
     */
    LomNativeOverlayDispatchStatus lom_native_overlay_run(const LomNativeOverlayInfo* info, intptr_t arg0, s32* result);

    /**
     * @brief Run GNAME's entry point with its original seven arguments.
     */
    LomNativeOverlayDispatchStatus lom_native_overlay_run_gname(const LomNativeOverlayInfo* info, intptr_t render_buffers, intptr_t initial_name,
                                                                intptr_t active_name, s32 source_mode, s32 history_index, intptr_t custom_name,
                                                                s32 allow_empty_cancel, s32* result);

    /** @brief The name the last overlay to start gave lom_overlay_require, or
     *  "" before any. For logs: after a menu runs another overlay and gets
     *  control back, it's still the other one's. */
    const char* lom_native_overlay_last_name(void);

#ifdef __cplusplus
}
#endif

#endif
