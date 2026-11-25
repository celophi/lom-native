#ifndef LOM_NATIVE_DISPLAY_H
#define LOM_NATIVE_DISPLAY_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Read the selected presentation filter.
     * @return Filter index in the supported presentation modes.
     */
    int lom_native_display_filter(void);

    /**
     * @brief Read the internal resolution actually in use.
     * @return Index 0, 1 or 2 for 1x, 2x or 4x rendering.
     */
    int lom_native_display_resolution(void);

    /**
     * @brief Read the current fullscreen selection.
     * @return True when fullscreen is enabled.
     */
    bool lom_native_display_fullscreen(void);

    /**
     * @brief Request display settings.
     * @param filter Supported presentation mode index; invalid values leave the filter unchanged.
     * @param resolution Resolution selection index 0, 1 or 2.
     * @param fullscreen Whether fullscreen should be enabled.
     */
    void lom_native_display_set(int filter, int resolution, bool fullscreen);

    /**
     * @brief Report whether a resolution change hasn't been applied yet.
     * @return True while display settings shouldn't be saved.
     */
    bool lom_native_display_pending(void);

    /**
     * @brief Read the error from the last failed resolution change.
     * @return Borrowed message, empty after success or a new request.
     */
    const char* lom_native_display_error(void);
#ifdef __cplusplus
}
#endif
#endif
