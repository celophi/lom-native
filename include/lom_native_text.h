#ifndef LOM_NATIVE_TEXT_H
#define LOM_NATIVE_TEXT_H

#ifdef __cplusplus
extern "C"
{
#endif

#define LOM_NATIVE_TEXT_WIDE_LEAD 0x20
#define LOM_NATIVE_TEXT_WIDE_BASE 0x1000
#define LOM_NATIVE_TEXT_WIDE_CODE(high, low) (LOM_NATIVE_TEXT_WIDE_BASE + ((((high) - LOM_NATIVE_TEXT_WIDE_LEAD) & 0xFF) << 7) + ((low) & 0x7F))

#define LOM_NATIVE_TEXT_BREAK_BEFORE 1
#define LOM_NATIVE_TEXT_BREAK_AFTER 2

    /**
     * @brief Say where FIELD's text may break a line around one of the CJK
     *        font's characters.
     * @param code The character's wide code (LOM_NATIVE_TEXT_WIDE_BASE and up).
     * @return LOM_NATIVE_TEXT_BREAK_BEFORE, LOM_NATIVE_TEXT_BREAK_AFTER, both,
     *         or 0 for a code that isn't a wide character.
     */
    int lom_native_text_breaks(int code);

    /**
     * @brief Check whether the Remaster's text is in use, so FIELD's text can
     *        have the port's extra letters and three-byte characters in it.
     *        With the PS1's text, those codes are the PS1's own and mean what
     *        they meant there.
     * @return 1 when it is.
     */
    int lom_native_text_in_use(void);

    /** @brief The glyph for @p code: 12 rows of 16 pixels, the leftmost in the top bit, as FIELD's font has them. NULL if there's none, or the PS1's text is in
     * use. */
    const unsigned short* lom_native_text_glyph(int code);

    /** @brief How far @p code advances, in pixels, or 0 if there's no glyph for it. */
    int lom_native_text_glyph_width(int code);

    /**
     * @brief An item's name in the language chosen, if it's one the game gave it (a template's, or a material and a type), as it's drawn.
     * @param name The name the saved game has, in the PS1's codes.
     * @return The name to draw: @p name itself if it isn't one of those, or the text is the PS1's.
     */
    const unsigned char* lom_native_text_item_name(const unsigned char* name);

    /** @brief Whether the language chosen names an item's type before its material ("Couteau de Ménos", "Daga Menos"): French and Spanish. */
    int lom_native_text_type_first(void);

    /** @brief The menu's text resource to use: @p resources, or a copy in the language chosen (MENU's 34 tables). */
    void* lom_native_text_menu(void* resources);

    /**
     * @brief SHOP's text resource to use: @p resources, or a copy in the
     *        language chosen (its 4 tables).
     * @param resources SHOP's own.
     * @return The one to use.
     */
    void* lom_native_text_shop(void* resources);

    /**
     * @brief GOSUB's text resource to use: @p resources, or a copy in the
     *        language chosen (its 12 tables).
     * @param resources GOSUB's own.
     * @return The one to use.
     */
    void* lom_native_text_gosub(void* resources);

    /** @brief FIELD's own small table of words (Buy, Lucre, You have...), one table on its own. */
    void* lom_native_text_field_ui(void* table);

    /** @brief The card screens' table of words (CARDA, CLOAD, ADDHERO, NIKI and MENU each have one), one table on its own. */
    void* lom_native_text_card(void* table);

    /** @brief ZUKAN's archive (two pictures, then the entry and category names), for its names only. */
    void* lom_native_text_zukan(void* archive);

    /**
     * @brief An encyclopedia page's 12 lines in the language chosen.
     * @param resource The page's disc resource (ANA/ZUKAN_P/Z0N/JTNNNN.DAT).
     * @param ps1 Its 12 lines as the PS1 has them, for their width.
     * @param lines Set to the 12 lines to draw instead.
     * @return 1 if @p lines is set, 0 to draw the PS1's.
     */
    int lom_native_text_encyclopedia(int resource, const unsigned char* const ps1[12], const unsigned char* lines[12]);

#ifdef __cplusplus
}
#endif
#endif
