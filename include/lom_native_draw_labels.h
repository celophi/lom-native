#ifndef LOM_NATIVE_DRAW_LABELS_H
#define LOM_NATIVE_DRAW_LABELS_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        LOM_LABEL_NONE = 0,
        LOM_LABEL_FIELD_ACTOR,
        LOM_LABEL_FIELD_EFFECT,
        LOM_LABEL_FIELD_TINT,
        LOM_LABEL_FIELD_LAYER,
        LOM_LABEL_PART,
        LOM_LABEL_KIND_COUNT
    } LomDrawLabelKind;

    typedef struct
    {
        int kind;
        int id;
        int detail;
        const char* name;
    } LomDrawLabel;

    /**
     * @brief Say what the packets from @p start up to @p end were built for.
     * @param kind A LomDrawLabelKind.
     * @param id Which one of that kind, as the kind says.
     * @param detail More about it, as the kind says.
     */
    void lom_native_draw_label(int kind, int id, int detail, const void* start, const void* end);

    /**
     * @brief Say the packets from @p start up to @p end are a part of a
     *        screen's frame.
     * @param name What it is, starting with the screen: "FIELD dialog". A
     *             string that lasts, since it's kept as given.
     * @param id Which one, where the part has several (a window's slot), or 0.
     */
    void lom_native_draw_label_part(const char* name, int id, const void* start, const void* end);

    /** @brief The label covering the packet at @p packet, or 0: a thing if
     *         one covers it, otherwise the part. */
    const LomDrawLabel* lom_native_draw_label_find(const void* packet);

    /** @brief A frame is done: the labels from the one before it go. The game
     *         loop calls it (lom_native_game_loop.h). */
    void lom_native_draw_labels_next_frame(void);

    /** @brief Forget every label. For a screen that draws each frame as soon
     *  as it's built, so only that frame's labels count. */
    void lom_native_draw_labels_clear(void);

    /** @brief With LOM_DRAW_LABEL_CHECK set, count the packets in the ordering
     *  table about to be drawn from @p ordering_table that have no label, by
     *  screen. The port's frame calls run it just before they draw. */
    void lom_native_draw_labels_check(const char* screen, const void* ordering_table);

    /** @brief Print what lom_native_draw_labels_check counted, if anything. */
    void lom_native_draw_labels_report(void);

#ifdef __cplusplus
}
#endif
#endif
