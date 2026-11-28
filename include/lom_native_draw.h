#ifndef LOM_NATIVE_DRAW_H
#define LOM_NATIVE_DRAW_H

#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        LOM_MENU_PROMPT_STATUS_VIEW,
        LOM_MENU_PROMPT_WEAPON,
        LOM_MENU_PROMPT_ARMOR,
        LOM_MENU_PROMPT_INSTRUMENT,
        LOM_MENU_PROMPT_COUNT
    };

    enum
    {
        LOM_CARDS_ROW_DARK,
        LOM_CARDS_ROW_SELECTED,
        LOM_CARDS_FACE_HERO,
        LOM_CARDS_FACE_HEROINE,
        LOM_CARDS_ARROW_UP,
        LOM_CARDS_ARROW_DOWN,
        LOM_CARDS_WINDOW,
        LOM_CARDS_PICTURE_COUNT
    };

    enum
    {
        LOM_CARDS_TEXT_ROW,
        LOM_CARDS_TEXT_TITLE,
        LOM_CARDS_TEXT_DIM,
        LOM_CARDS_TEXT_FAINT,
        LOM_CARDS_TEXT_COUNT
    };

    enum
    {
        LOM_CARDS_ALIGN_LEFT,
        LOM_CARDS_ALIGN_RIGHT,
        LOM_CARDS_ALIGN_CENTER
    };

    enum
    {
        LOM_TITLE_MENU_COPYRIGHT,
        LOM_TITLE_MENU_ITEM,
        LOM_TITLE_MENU_CURSOR
    };

    /**
     * @brief Draw one of FIELD's actor or effect sprites.
     * @param list The drawing list entry for its depth.
     * @param quad Its screen quad, a textured four-point packet the game built.
     * @param resource The FIELD resource it's from.
     * @param frame Its frame, in the resource's frame data.
     * @param x Where it is in the room, across.
     * @param y Up.
     * @param z And into the room, which is also what sorts it.
     */
    void lom_draw_field_sprite(void* list, void* quad, int resource, const void* frame, int x, int y, int z);

    /**
     * @brief Draw a run of one of FIELD's background layers' tiles.
     * @param list The drawing list entry for the run's shade.
     * @param first The run's first packet; the game has chained them.
     * @param last Its last.
     * @param part The layer part they're from (FieldPart).
     * @param shade Which of the layer's palettes it's drawn with, from the
     *              room's lighting.
     */
    void lom_draw_field_tiles(void* list, void* first, void* last, const void* part, int shade);

    /**
     * @brief The room's map is loaded and its images are in video memory,
     *        recolored if the scene asked (field_apply_pixel_lookup), so a
     *        renderer drawing the room from other art can do the same.
     * @param lookup The color table the images went through: 32 colors, one
     *               per level of a pixel's brightest component, 15-bit PS1
     *               colors. NULL when they weren't recolored.
     */
    void lom_draw_field_map_ready(const unsigned short* lookup);

    /**
     * @brief Say which tile a background tile record was built from, each
     *        time the game builds or rebuilds one, so a renderer can draw it
     *        from other art (the Remaster's).
     * @param desc The tile's four-byte descriptor in the room's map.
     * @param record The record built from it (FieldTileRec).
     */
    void lom_draw_field_tile_record(const void* desc, const void* record);

    /**
     * @brief Say a tile record was copied over another, as FIELD's tile
     *        animations do each frame from the records they built for every
     *        frame, so the copy is drawn as the tile it now is.
     * @param to The record copied into (FieldTileRec).
     * @param from The record it was copied from.
     */
    void lom_draw_field_tile_copied(const void* to, const void* from);

    /**
     * @brief How many more pixels to scroll a scenery layer sideways, for a
     *        renderer whose art was placed for another camera (the
     *        Remaster's). 0 when there's none.
     * @param scale How the layer scrolls with the camera, in sixteenths,
     *              bit 7 for the other way (FieldObjDef's scroll scales).
     * @param camera_x The camera as FIELD keeps it: minus how far the view
     *                 has scrolled across the room, in pixels.
     * @param room_width The room's size (FieldSceneHeader's map_width and
     * @param room_depth map_depth), to tell which of the map's rooms it is.
     */
    int lom_draw_field_layer_shift(int scale, int camera_x, int room_width, int room_depth);

    /**
     * @brief How far across the room's art reaches, when it's drawn from art
     *        that reaches past the room (the Remaster's, made for a wide
     *        view), so a wide view can show it.
     * @param room_width The room's size (FieldSceneHeader's map_width and
     * @param room_depth map_depth), to tell which of the map's rooms it is.
     * @param left Set to where the art starts, in pixels from the room's left.
     * @param right Set to where it ends.
     * @return 1 when there's art past the room, else 0 and the room is all.
     */
    int lom_draw_field_room_extent(int room_width, int room_depth, int* left, int* right);

    /**
     * @brief Let the renderer take over one background tile's packet, just
     *        built from @p record, before it's chained into a run.
     * @param code The packet's code word: a 16 pixel sprite's (then its
     *             position and texture words) or a four-point textured
     *             polygon's (then its points and texture words in turn).
     * @param record The tile record it was built from.
     */
    void lom_draw_field_tile(void* code, const void* record);

    /**
     * @brief Draw the picture on an encyclopedia page, which the game has
     *        built as sprites and linked, so a renderer can draw it from
     *        other art (the Remaster's).
     * @param code The first sprite's code word (its color and code).
     * @param stride How far each next sprite's is, in bytes.
     * @param count How many sprites the picture is.
     * @param resource The disc resource the page came from.
     */
    void lom_draw_encyclopedia_picture(void* code, int stride, int count, int resource);

    /**
     * @brief WSEL, the world select, has loaded its pictures into video
     *        memory, so a renderer can get its own ready too.
     */
    void lom_draw_wsel_loaded(void);

    /**
     * @brief Draw one of WSEL's pictures, which the game has built as sprites
     *        and linked, so a renderer can draw it from other art.
     * @param code The first sprite's code word.
     * @param stride How far each next sprite's is, in bytes.
     * @param count How many sprites.
     * @param picture Which of WSEL's pictures it is (WSEL_SPRITE_*).
     * @param shadow Nonzero for the cursor's drop shadow, drawn as a second
     *        pass of the same picture.
     * @param x Where the picture's top left is on the screen.
     * @param y The same, down.
     */
    void lom_draw_wsel_picture(void* code, int stride, int count, int picture, int shadow, int x, int y);

    /**
     * @brief Draw the title's backdrop, which the game has built as quads and
     *        linked, so a renderer can draw it from other art.
     * @param code The first quad's code word.
     * @param stride How far each next quad's is, in bytes.
     * @param count How many quads.
     */
    void lom_draw_title_backdrop(void* code, int stride, int count);

    /**
     * @brief Draw a part of the title screen's menu, which the game has built
     *        and linked as one quad, so a renderer can draw it from other art.
     * @param code The quad's code word.
     * @param part LOM_TITLE_MENU_*.
     * @param index For an item, which: 0 New Game, 1 Continue.
     * @param selected For an item, nonzero when it's the one chosen.
     */
    void lom_draw_title_menu(void* code, int part, int index, int selected);

    /** @brief Start a save or load screen, deciding whether it's laid out as the Remaster's. */
    void lom_draw_cards_begin(void);

    /** @brief End a save or load screen. */
    void lom_draw_cards_end(void);

    /** @brief Nonzero when the save and load screens are laid out as the Remaster's. */
    int lom_draw_cards_on(void);

    /** @brief Start one of a save or load screen's frames, wide or not. */
    void lom_draw_cards_frame(void);

    /**
     * @brief Draw a save or load screen's backdrop sprites, which the game has
     *        built and linked, so a renderer can draw them from other art.
     * @param code The first sprite's code word.
     * @param stride How far each next sprite's is, in bytes.
     * @param count How many sprites.
     */
    void lom_draw_cards_background(void* code, int stride, int count);

    /**
     * @brief Draw one of the pictures of the Remaster's save and load screens
     *        over a sprite the game has built and linked.
     * @param code The sprite's code word.
     * @param picture LOM_CARDS_*.
     * @param x Where it goes: its left.
     * @param y Its top.
     * @param w Its width.
     * @param h Its height.
     */
    void lom_draw_cards_picture(void* code, int picture, int x, int y, int w, int h);

    /**
     * @brief Write a line of a save or load screen's text in the Remaster's
     *        font, over a sprite the game has built but not linked.
     * @param code The sprite's code word.
     * @param text The game's text.
     * @param x Where: its left, right or middle (@p align).
     * @param baseline The line's baseline.
     * @param align LOM_CARDS_ALIGN_*.
     * @param style LOM_CARDS_TEXT_*.
     * @return Nonzero when it's written; 0 to draw it in the PS1's font, as
     *         when the Remaster's font can't write it.
     */
    int lom_draw_cards_text(void* code, const unsigned char* text, int x, int baseline, int align, int style);

    /**
     * @brief Draw a fade to black over a sprite the game has built but not
     *        linked, darkening in proportion rather than as the PS1, which
     *        takes the same amount off every colour.
     * @param code The sprite's code word.
     * @param brightness The fade's level: 0x100 not faded, 0 black.
     * @param x The faded area's left.
     * @param y Its top.
     * @param w Its width.
     * @param h Its height.
     * @return Nonzero when it's drawn this way (with the Remaster in use);
     *         0 to draw the PS1's fade.
     */
    int lom_draw_fade(void* code, int brightness, int x, int y, int w, int h);

    /** @brief Nonzero while the game's menu writes its text in the Remaster's fonts. */
    int lom_draw_menu_hd(void);

    /**
     * @brief Write a line of the menu's text in the Remaster's fonts, over a
     *        sprite the game has built but not linked.
     * @param code The sprite's code word, or NULL only to measure it.
     * @param text The game's text.
     * @param colour The PS1's text style, as field_draw_text takes it.
     * @param x Its left, right or middle (@p align).
     * @param baseline Its baseline.
     * @param align LOM_CARDS_ALIGN_*, which match FIELD_TEXT_ALIGN_*.
     * @param left Set to its left, or NULL.
     * @param width Set to its width, or NULL.
     * @return Nonzero when it's written; 0 to draw it in the PS1's font.
     */
    int lom_draw_menu_text(void* code, const unsigned char* text, int colour, int x, int baseline, int align, int* left, int* width);

    /**
     * @brief Draw name entry's fixed background sprites, which the game has
     *        built and linked, so a renderer can draw them from other art.
     * @param code The first sprite's code word.
     * @param stride How far each next sprite's is, in bytes.
     * @param count How many sprites.
     */
    void lom_draw_gname_background(void* code, int stride, int count);

    /**
     * @brief Draw one of name entry's glyphs, which the game has built and
     *        linked, so a renderer can draw or place it otherwise.
     * @param code The glyph's sprite's code word.
     * @param sprites How many sprites: 2 when it has a shadow after it, else 1.
     * @param stride How far the shadow's code word is, in bytes.
     * @param glyph The glyph.
     */
    void lom_draw_gname_glyph(void* code, int sprites, int stride, int glyph);

    /**
     * @brief Where name entry's buttons and tabs are, for a layout that moves
     *        them: nonzero when its buttons are a column between the tabs and
     *        the letters, so they're stepped through up and down.
     */
    int lom_draw_gname_column(void);

    /** @brief Start the game's menu, deciding whether it's laid out as the Remaster's. */
    void lom_draw_menu_begin(void);

    /** @brief End the game's menu. */
    void lom_draw_menu_end(void);

    /** @brief Start one of the menu's frames, wide or not. */
    void lom_draw_menu_frame(void);

    /** @brief Nonzero when the menu's tabs are in two columns, the Remaster's layout. */
    int lom_draw_menu_columns(void);

    /**
     * @brief Where a tab goes in the two-column layout.
     * @param column 0 or 1 for the columns, 2 for Return's box.
     * @param row Its row.
     * @param x Set to its left on the screen.
     * @param y Set to its top.
     */
    void lom_draw_menu_tab_xy(int column, int row, int* x, int* y);

    /**
     * @brief Draw the menu's frame sprites, which the game has built and
     *        linked, so a renderer can draw them from other art.
     * @param code The first sprite's code word.
     * @param stride How far each next sprite's is, in bytes.
     * @param count How many sprites.
     */
    void lom_draw_menu_background(void* code, int stride, int count);

    /**
     * @brief Draw one of the menu's icons, which the game has built and
     *        linked, so a renderer can draw it from other art.
     * @param code The icon's sprite's code word.
     * @param sprites How many sprites: 2 with its highlight or shadow, else 1.
     * @param stride How far the second one's code word is, in bytes.
     * @param icon The icon.
     * @param mirror Nonzero when it repeats the open page at the top of the
     *        second column.
     */
    void lom_draw_menu_icon(void* code, int sprites, int stride, int icon, int mirror);

    /**
     * @brief Draw a button's key cap with its name in the menu, from four
     *        sprites the game has built and linked (the name's first, so it's
     *        drawn over the cap's three), so a renderer can draw it from other
     *        art.
     * @param code The first sprite's code word.
     * @param stride How far each next one's is, in bytes.
     * @param x The cap's left.
     * @param y Its top.
     * @param w Its width.
     * @param h Its height.
     * @param name The button's name.
     */
    void lom_draw_menu_key(void* code, int stride, int x, int y, int w, int h, const char* name);

    /**
     * @brief The words beside a button prompt in the menu, in the game's own
     *        text, or NULL to show the PS1's button icons.
     * @param which LOM_MENU_PROMPT_*.
     * @return The text.
     */
    const unsigned char* lom_draw_menu_prompt(int which);

    /** @brief How far across name entry's tabs and the label above them are moved. */
    int lom_draw_gname_tab_shift(void);

    /**
     * @brief Move name entry's cursor target for a button or tab to where the
     *        layout has it.
     * @param position The target's position in the screen's table.
     * @param x Its x, moved.
     * @param y Its y, moved.
     */
    void lom_draw_gname_cursor(int position, int* x, int* y);

    /**
     * @brief Draw WSEL's zoom, the map on the wall stretched over a textured
     *        four-point packet the game built and linked.
     * @param code The packet's code word.
     */
    void lom_draw_wsel_zoom(void* code);

    enum
    {
        LOM_DRAW_WMAP_SHELL,
        LOM_DRAW_WMAP_DAY_ICON,
        LOM_DRAW_WMAP_DAY_NAME
    };

    /**
     * @brief Draw a part of the world map's status corner, the day's spirit
     *        in its shell and the day's name, which the game has built as a
     *        sprite and linked.
     * @param code The sprite's code word.
     * @param part Which part (LOM_DRAW_WMAP_*). Which day it shows is in the
     *        sprite's texture position.
     */
    void lom_draw_wmap_status(void* code, int part);

    /**
     * @brief Draw a land's name on the world map, which the game has built as
     *        a sprite and linked.
     * @param code The sprite's code word.
     * @param selection The land, or 64 plus the land for the name of the
     *        artifact that makes it.
     * @param tpage The texture page the name is on.
     */
    void lom_draw_wmap_label(void* code, int selection, int tpage);

    /**
     * @brief Draw the world map's panel, the disc with the Mana Tree on it in
     *        the corner, which the game has built as a textured four-point
     *        packet and linked.
     * @param code The packet's code word.
     * @param stage How far the tree has grown, from 0, which picks the
     *        panel's picture.
     */
    void lom_draw_wmap_panel(void* code, int stage);

    enum
    {
        LOM_DRAW_WMAP_SPIRIT_ICON,
        LOM_DRAW_WMAP_SPIRIT_LEVEL
    };

    /**
     * @brief Draw one of the world map's spirits, or how strong it is in a
     *        place, which the game has built as a sprite and linked.
     * @param code The sprite's code word.
     * @param spirit Which spirit, in the game's order: Wisp, Shade, Dryad,
     *        Aura, Salamander, Gnome, Jinn, Undine.
     * @param kind LOM_DRAW_WMAP_SPIRIT_*. Which frame or level it shows is in
     *        the sprite's texture position.
     */
    void lom_draw_wmap_spirit(void* code, int spirit, int kind);

#ifdef __cplusplus
}
#endif
#endif
