#ifndef LOM_NATIVE_FIELD_WIDESCREEN_H
#define LOM_NATIVE_FIELD_WIDESCREEN_H

#ifdef __cplusplus
extern "C"
{
#endif

    struct FieldObj;
    struct FieldPart;

    typedef enum
    {
        LOM_FIELD_WIDESCREEN_WIDE,
        LOM_FIELD_WIDESCREEN_DISABLED,
        LOM_FIELD_WIDESCREEN_NO_SCENE,
        LOM_FIELD_WIDESCREEN_NARROW,
        LOM_FIELD_WIDESCREEN_MOVIE,
        LOM_FIELD_WIDESCREEN_DUEL,
        LOM_FIELD_WIDESCREEN_UNSUPPORTED_LAYER,
        LOM_FIELD_WIDESCREEN_HIDDEN_EDGE,
        LOM_FIELD_WIDESCREEN_STATUS_COUNT
    } LomFieldWidescreenStatus;

    /**
     * @brief Decide whether the current room can be drawn wide.
     * @param rejected_object Receives the index of the layer that keeps it 4:3, or -1. May be null.
     * @return The decision and its reason.
     */
    LomFieldWidescreenStatus lom_native_field_widescreen_status(int* rejected_object);

    /**
     * @brief Start a FIELD frame: decide, log a changed decision, and set the frame wide or not.
     *        Call before the frame builds any drawing.
     */
    void lom_native_field_widescreen_begin_frame(void);

    /**
     * @brief Back to the original view, when FIELD stops drawing (a scene change, leaving FIELD).
     */
    void lom_native_field_widescreen_end(void);

    /**
     * @brief The camera to draw scenery and actors with this frame.
     * @param camera_x The game's camera, in 1/256 pixels (a translation, so negative).
     * @return The same camera, kept far enough from the map's ends that the wide
     *         view stays inside the map; unchanged when the frame isn't wide.
     */
    int lom_native_field_widescreen_camera_x(int camera_x);

    /**
     * @brief Place a slow parallax layer's part so the wide view never shows an edge the original never shows.
     * @param object The part's object.
     * @param part The part.
     * @param x The part's screen x as the game placed it.
     * @param scaled Zero in the emitter's unscaled mode, which draws every layer at full speed.
     * @return The x to draw it at.
     */
    int lom_native_field_widescreen_layer_x(const struct FieldObj* object, const struct FieldPart* part, int x, int scaled);

    /**
     * @brief Draw the actors with the game's own camera, then move their packets
     * sideways to where the wide frame's camera puts them.
     * @param render The frame's FieldRenderContext.
     * @param alternate The frame's render half.
     */
    void lom_native_field_widescreen_draw_actors(void* render, int alternate);

    /**
     * @brief A packet cursor in the port's own buffer, for the extra copy of a
     * wrapping layer the wide view needs on its left. The game's packet buffer
     * is sized for the copies it draws itself, so this one goes elsewhere.
     * @param cells The part's cell count (FieldPart::instance_count).
     * @return The cursor to draw through, or NULL when not wide or out of room.
     */
    unsigned char** lom_native_field_widescreen_wrap_cursor(unsigned cells);

    /** @brief Done drawing through the cursor wrap_cursor gave. */
    void lom_native_field_widescreen_wrap_done(void);

    /**
     * @brief Continue a fixed sky's edge tile into the wide view's side strips.
     * @param part The part the sprite belongs to.
     * @param last_primitive The SPRT_16 just emitted for it.
     * @param next The address the sprite links to (the game's next packet).
     * @param origin_y The part's top row on screen.
     * @return The last packet of the chain now ending in @p next: the sprite
     *         itself, or the last copy, which live in the port's own buffer.
     */
    void* lom_native_field_widescreen_extend_background(const struct FieldPart* part, void* last_primitive, void* next, int origin_y);

#ifdef __cplusplus
}
#endif

#endif
