#ifndef LOM_NATIVE_REMASTER_H
#define LOM_NATIVE_REMASTER_H

#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        LOM_REMASTER_MUSIC_PS1,
        LOM_REMASTER_MUSIC_ORIGINAL,
        LOM_REMASTER_MUSIC_ARRANGED,
        LOM_REMASTER_MUSIC_COUNT
    };

    /**
     * @brief Look for the Remaster and, if it's there, hook it in. The
     *        launcher passes its folder in LOM_REMASTER_ROOT (empty when the
     *        player turned it off); without that, the usual Steam folders
     *        are tried. LOM_REMASTER_HD (0 or 1) and LOM_REMASTER_MUSIC (0 to
     *        2) override the settings, for runs without them. The Japanese
     *        game says here that it can't show the Remaster's text.
     * @return Nonzero if it was found.
     */
    int lom_native_remaster_initialize(void);

    /** @brief Once the disc and the sound driver are up: start the music's
     *         side, if the Remaster was found. */
    void lom_native_remaster_start_music(void);

    /** @brief Nonzero when the Remaster was found, so its menu rows show. */
    int lom_native_remaster_found(void);

    /** @brief Its data folder (the one with hdmap/ in it), or "" if it wasn't found. */
    const char* lom_native_remaster_root(void);

    /** @brief Nonzero when the renderer can draw the Remaster's backgrounds (src/gpu/hd_textures.h). */
    int lom_native_remaster_hd_supported(void);

    /** @brief Draw rooms with the Remaster's backgrounds, from the next room on. */
    void lom_native_remaster_set_hd_backgrounds(int enabled);

    /** @brief Nonzero when rooms are drawn with the Remaster's backgrounds. */
    int lom_native_remaster_hd_backgrounds(void);

    /** @brief Where the music comes from (LOM_REMASTER_MUSIC_*), from the next song on. */
    void lom_native_remaster_set_music(int source);

    /** @brief Where the music comes from: LOM_REMASTER_MUSIC_*. */
    int lom_native_remaster_music(void);

    /** @brief Whether sound effects come from the Remaster's recordings, from the next effect on. */
    void lom_native_remaster_set_effects(int enabled);

    /** @brief Nonzero when sound effects come from the Remaster's recordings. */
    int lom_native_remaster_effects(void);

    /** @brief The game plays effect data (src/audio/sound_boundary.c). */
    void lom_remaster_effects_data(const void* data);

    /** @brief The game asked for effects to stop (src/audio/sound_boundary.c). */
    void lom_remaster_effects_stopped(void);

    enum
    {
        LOM_REMASTER_TEXT_PS1,
        LOM_REMASTER_TEXT_ENGLISH,
        LOM_REMASTER_TEXT_GERMAN,
        LOM_REMASTER_TEXT_FRENCH,
        LOM_REMASTER_TEXT_SPANISH,
        LOM_REMASTER_TEXT_JAPANESE,
        LOM_REMASTER_TEXT_KOREAN,
        LOM_REMASTER_TEXT_CHINESE_SIMPLIFIED,
        LOM_REMASTER_TEXT_CHINESE_TRADITIONAL,
        LOM_REMASTER_TEXT_COUNT
    };

    /** @brief Which text the scenes use (LOM_REMASTER_TEXT_*), from the next scene on. */
    void lom_native_remaster_set_text(int language);

    /** @brief Which text the player chose: LOM_REMASTER_TEXT_*. */
    int lom_native_remaster_text(void);

    /**
     * @brief Say whether this game can show the Remaster's text. The Japanese
     *        game can't: its text is encoded differently, so it keeps the
     *        disc's own. The player's choice is kept either way, for the other
     *        game, which shares the preferences.
     * @param supported Nonzero when it can; it can until this says otherwise.
     */
    void lom_native_remaster_set_text_supported(int supported);

    /**
     * @brief Check whether this game can show the Remaster's text.
     * @return 1 when it can.
     */
    int lom_native_remaster_text_supported(void);

    /**
     * @brief Which text the scenes use: the player's choice, or the PS1's when
     *        this game can't show the Remaster's.
     * @return LOM_REMASTER_TEXT_*.
     */
    int lom_native_remaster_text_in_use(void);

    /** @brief Nonzero when the renderer can draw the Remaster's UI pictures (src/gpu/hd_textures.h). */
    int lom_native_remaster_hd_ui_supported(void);

    /** @brief Draw the Remaster's repainted pictures where the PS1's screens have them, from the next frame on. */
    void lom_native_remaster_set_hd_ui(int enabled);

    /** @brief Nonzero when the Remaster's repainted pictures are drawn. */
    int lom_native_remaster_hd_ui(void);

    /**
     * @brief Remember which of the Remaster's tiles a background tile record
     *        stands for (lom_draw_field_tile_record).
     * @param desc The tile's four-byte descriptor in the room's map.
     * @param record The FieldTileRec built from it.
     */
    void lom_remaster_tile_record(const void* desc, const void* record);

    /**
     * @brief Carry the tile a record stands for over to a record copied from it
     *        (lom_draw_field_tile_copied).
     * @param to The FieldTileRec copied into.
     * @param from The one it was copied from.
     */
    void lom_remaster_tile_copied(const void* to, const void* from);

    /**
     * @brief How many more pixels to scroll a scenery layer, in a room whose
     *        Remaster art was placed for a wider view (lom_draw_field_layer_shift).
     * @param scale How the layer scrolls with the camera, in sixteenths, bit 7
     *        for the other way.
     * @param camera_x Minus how far the view has scrolled across the room.
     * @param room_width The room's width, to tell which room it is.
     * @param room_depth The room's depth.
     * @return The extra pixels, or 0.
     */
    int lom_remaster_layer_shift(int scale, int camera_x, int room_width, int room_depth);

    /**
     * @brief How far the Remaster's art for a room reaches past the PS1's
     *        (lom_draw_field_room_extent).
     * @param room_width The room's width, to tell which room it is.
     * @param room_depth The room's depth.
     * @param left Set to where the art starts, in pixels from the room's left.
     * @param right Set to where it ends.
     * @return 1 when it reaches past the room, otherwise 0.
     */
    int lom_remaster_room_extent(int room_width, int room_depth, int* left, int* right);

    /**
     * @brief Upload the Remaster's background pages for the room just loaded,
     *        recolored the way the scene recolored the PS1's
     *        (lom_draw_field_map_ready). In a room without Remaster art, drop
     *        them.
     * @param lookup The scene's color table, or NULL.
     */
    void lom_remaster_map_ready(const unsigned short* lookup);

    /**
     * @brief Turn a background tile's packet into a draw of the Remaster's
     *        tile, when the record stands for one (lom_draw_field_tile).
     * @param code The packet's code word.
     * @param record The FieldTileRec it was built from.
     */
    void lom_remaster_tile(void* code, const void* record);

    /**
     * @brief Draw the Remaster's picture for an encyclopedia page over the
     *        sprites the game built for it (lom_draw_encyclopedia_picture).
     * @param code The first sprite's code word.
     * @param stride How far each next sprite's is, in bytes.
     * @param count How many sprites.
     * @param path The page's file on the disc.
     */
    void lom_remaster_encyclopedia_picture(void* code, int stride, int count, const char* path);

    /** @brief In how many lines of pixels the Remaster's picture for the encyclopedia page @p path covers each screen
     *  column, on each of @p rows rows of text (from @p top, every @p spacing, @p height high), into @p cover (@p rows by
     *  @p width). 0 when the page draws the PS1's picture instead. */
    int lom_remaster_encyclopedia_cover(const char* path, int top, int spacing, int height, int rows, int width, unsigned char* cover);

#ifdef __cplusplus
}
#endif
#endif
