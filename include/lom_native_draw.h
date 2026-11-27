#ifndef LOM_NATIVE_DRAW_H
#define LOM_NATIVE_DRAW_H

#ifdef __cplusplus
extern "C"
{
#endif

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
