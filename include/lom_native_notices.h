#ifndef LOM_NATIVE_NOTICES_H
#define LOM_NATIVE_NOTICES_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_NOTICE_MOD_REFUSED,
        LOM_NOTICE_MOD_STOPPED,
    } LomNoticeKind;

    /**
     * @brief Tell the player something on screen, over the game, for a short
     *        while. The F1 menu draws them while it's closed. Only the latest
     *        few are kept.
     * @param kind What happened; the menu words it in the player's language.
     * @param subject What it happened to, like a mod's id. Copied, and cut
     *        short if it's long.
     * @param detail Why, in English, or NULL. Only its first line is kept.
     */
    void lom_native_notice_post(LomNoticeKind kind, const char* subject, const char* detail);

    /**
     * @brief Count the notices kept, oldest first.
     * @return How many.
     */
    int lom_native_notice_count(void);

    /**
     * @brief Read a notice's kind.
     * @param index From 0 to lom_native_notice_count() - 1.
     * @return Its kind; LOM_NOTICE_MOD_REFUSED for an index out of range.
     */
    LomNoticeKind lom_native_notice_kind(int index);

    /**
     * @brief Read what a notice is about.
     * @param index From 0 to lom_native_notice_count() - 1.
     * @return The subject, or "" for an index out of range.
     */
    const char* lom_native_notice_subject(int index);

    /**
     * @brief Read why, for a notice.
     * @param index From 0 to lom_native_notice_count() - 1.
     * @return The first line of the detail, or "" when there's none or the
     *         index is out of range.
     */
    const char* lom_native_notice_detail(int index);

    /**
     * @brief Read a notice's number, which counts up from 1 as notices are
     *        posted, so whoever shows them can tell a new one from an old one.
     * @param index From 0 to lom_native_notice_count() - 1.
     * @return The number, or 0 for an index out of range.
     */
    unsigned lom_native_notice_number(int index);

#ifdef __cplusplus
}
#endif
#endif
