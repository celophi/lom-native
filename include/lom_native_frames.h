#ifndef LOM_NATIVE_FRAMES_H
#define LOM_NATIVE_FRAMES_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief TITLE's ClearOTagR: run the tests' title screen routes, then
     *        clear the ordering table.
     * @param ordering_table The table's last entry, as ClearOTagR takes it.
     * @param entry_count How many entries it has: 0x1000 for a TITLE frame, 4
     *        for the screen transition.
     * @return What ClearOTagR returns.
     */
    void* lom_native_title_ClearOTagR(void* ordering_table, int entry_count);

    /**
     * @brief TITLE's DrawOTag: check the frame's packets, draw them, present
     *        and count the frame, and end the menu when the player has asked
     *        to quit.
     * @param ordering_table The table's last entry.
     */
    void lom_native_title_DrawOTag(void* ordering_table);

    /**
     * @brief Start one of TITLE's frames, before any of its drawing: wide for
     *        the title menu when Widescreen is on and its backdrop is the
     *        Remaster's, which is painted wide, and otherwise the original
     *        view, since the rest of TITLE's screens aren't.
     * @param menu Nonzero for one of the title menu's frames.
     */
    void lom_native_title_begin_frame(int menu);

    /**
     * @brief TITLE's PutDispEnv: show the buffer, and note for the tests which
     *        of the two it was.
     * @param env The DISPENV.
     * @return @p env.
     */
    void* lom_native_title_PutDispEnv(void* env);

    /**
     * @brief TITLE's PutDrawEnv: set where it draws, and note which buffer.
     * @param env The DRAWENV.
     * @return @p env.
     */
    void* lom_native_title_PutDrawEnv(void* env);

    /**
     * @brief GNAME's ClearOTagR, which the FIELD screens that borrow GNAME's
     *        frame calls use too.
     * @param ordering_table The table's last entry.
     * @param entry_count How many entries it has.
     * @return What ClearOTagR returns.
     */
    void* lom_native_gname_ClearOTagR(void* ordering_table, int entry_count);

    /**
     * @brief GNAME's DrawOTag: check the packets under the name of the overlay
     *        that's running, draw them, then present the frame.
     * @param ordering_table The table's last entry.
     */
    void lom_native_gname_DrawOTag(void* ordering_table);

    /**
     * @brief WSEL's ClearOTagR.
     * @param ordering_table The table's last entry.
     * @param count How many entries it has.
     * @return @p ordering_table.
     */
    void* lom_native_wsel_ClearOTagR(void* ordering_table, int count);

    /**
     * @brief WSEL's DrawOTag: check the packets, draw them, present the frame,
     *        and leave WSEL for the title screen when the player has asked to
     *        quit.
     * @param ordering_table The table's last entry.
     */
    void lom_native_wsel_DrawOTag(void* ordering_table);

    /**
     * @brief WMAP's ClearOTagR.
     * @param ot The table's last entry.
     * @param count How many entries it has.
     * @return @p ot.
     */
    void* lom_native_wmap_ClearOTagR(void* ot, int count);

    /**
     * @brief WMAP's DrawOTag: check the frame is one of WMAP's two and its
     *        packets stayed inside their buffer, draw it, present it, and stop
     *        the world map when the player quits.
     * @param ot The table's last entry.
     */
    void lom_native_wmap_DrawOTag(const void* ot);

    /**
     * @brief Start one of FIELD's frames: leave FIELD if the player has asked
     *        to quit, then run the tests' frame hook.
     */
    void lom_native_field_frame_begin(void);

    /**
     * @brief Draw and present one of FIELD's frames.
     * @param render The FieldRenderHalf the frame was built in.
     * @param half Which of the two halves it is, which sets the display's
     *        position.
     */
    void lom_native_field_show_frame(void* render, unsigned int half);

    /**
     * @brief End one of FIELD's frames: count it, and stop FIELD when a test's
     *        frame limit or stop condition is reached.
     */
    void lom_native_field_frame_end(void);

    /**
     * @brief GOVER's ClearOTagR.
     * @param table The table's last entry.
     * @param count How many entries it has.
     * @return @p table.
     */
    void* lom_native_gover_ClearOTagR(void* table, int count);

    /**
     * @brief GOVER's DrawOTag: draw and present the frame through GNAME's.
     * @param tail The table's last entry.
     */
    void lom_native_gover_DrawOTag(void* tail);

    /**
     * @brief GOVER's PutDrawEnv.
     * @param raw_env The DRAWENV.
     * @return What PutDrawEnv returns.
     */
    void* lom_native_gover_PutDrawEnv(void* raw_env);

#ifdef __cplusplus
}
#endif
#endif
