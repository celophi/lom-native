#ifndef LOM_NATIVE_REMASTER_H
#define LOM_NATIVE_REMASTER_H

#include <stdio.h>

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

    /** @brief Nonzero when the renderer can show the Remaster's movies at its own resolution (src/gpu/hd_movie.h). */
    int lom_native_remaster_hd_movies_supported(void);

    /** @brief Play the Remaster's HD movies in place of the PS1's, from the next movie on. */
    void lom_native_remaster_set_hd_movies(int enabled);

    /** @brief Nonzero when the Remaster's HD movies play in place of the PS1's. */
    int lom_native_remaster_hd_movies(void);

    /**
     * @brief The Remaster's file for one of the movies the movie screen plays,
     *        by the game's number for it (0 the opening, 1 the one after you
     *        place your home, 2 the first ending, 3 the staff roll, 4 the last
     *        ending), or NULL when it has none.
     * @param movie The movie's number.
     * @return Its file under the Remaster's data folder.
     */
    const char* lom_native_remaster_movie_file(unsigned movie);

    /**
     * @brief Where to play one of the movie screen's movies from instead of the
     *        disc: the Remaster's file, when it was found, HD movies are on,
     *        the renderer can show it and the file is there.
     * @param movie The movie's number, as lom_native_remaster_movie_file.
     * @param path Set to the file's path.
     * @param size Room in @p path.
     * @return Nonzero when the Remaster's movie plays.
     */
    int lom_native_remaster_movie_path(unsigned movie, char* path, unsigned long size);

    /**
     * @brief Write a Remaster movie's sound, when it isn't in the movie, as an
     *        xWMA file ffmpeg reads. Only the one after you place your home has
     *        this: its MP4's sound is silence, and the Remaster plays the
     *        music from its own track (sound/music_0013.psb), which starts with
     *        the picture.
     * @param movie The movie's number, as lom_native_remaster_movie_file.
     * @param file Where to write it: an empty file opened for writing.
     * @return 1 when it wrote it, 0 when the movie's own sound plays, or -1
     *         when the track can't be read or written.
     */
    int lom_native_remaster_movie_sound(unsigned movie, FILE* file);

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

    /** @brief Load WSEL's pictures, when HD pictures are on, so its zoom doesn't wait for them (lom_draw_wsel_loaded). */
    void lom_remaster_wsel_loaded(void);

    /**
     * @brief Draw the Remaster's art for one of WSEL's pictures over the
     *        sprites the game built for it (lom_draw_wsel_picture).
     * @param code The first sprite's code word.
     * @param stride How far each next sprite's is, in bytes.
     * @param count How many sprites.
     * @param picture Which picture (WSEL_SPRITE_*).
     * @param shadow Nonzero for the cursor's drop shadow.
     * @param x Where its top left is on the screen.
     * @param y The same, down.
     */
    void lom_remaster_wsel_picture(void* code, int stride, int count, int picture, int shadow, int x, int y);

    /** @brief Draw the Remaster's map in place of WSEL's zoom quad (lom_draw_wsel_zoom). @param code The quad's code word. */
    void lom_remaster_wsel_zoom(void* code);

    /** @brief Nonzero when WSEL's wall is the Remaster's, which is painted wide enough for a wide frame. */
    int lom_remaster_wsel_wide(void);

    enum
    {
        LOM_REMASTER_WMAP_SHELL,
        LOM_REMASTER_WMAP_DAY_ICON,
        LOM_REMASTER_WMAP_DAY_NAME
    };

    enum
    {
        LOM_REMASTER_WMAP_SPIRIT_ICON,
        LOM_REMASTER_WMAP_SPIRIT_LEVEL
    };

    /** @brief Draw the Remaster's art for a part of the world map's status corner (lom_draw_wmap_status). @param code The sprite's
     *  code word. @param part LOM_REMASTER_WMAP_*. */
    void lom_remaster_wmap_status(void* code, int part);

    /** @brief Draw the Remaster's banner for a land's name on the world map (lom_draw_wmap_label). @param code The sprite's code
     *  word. @param selection The land, or 64 plus it for its artifact's name. @param tpage The name's texture page. */
    void lom_remaster_wmap_label(void* code, int selection, int tpage);

    /** @brief Draw the Remaster's panel in place of the world map's (lom_draw_wmap_panel). @param code The quad's code word.
     *  @param stage The tree's growth, from 0. */
    void lom_remaster_wmap_panel(void* code, int stage);

    /** @brief Draw the Remaster's art for a world map spirit or its strength (lom_draw_wmap_spirit). @param code The sprite's
     *  code word. @param spirit Which spirit. @param kind LOM_REMASTER_WMAP_SPIRIT_*. */
    void lom_remaster_wmap_spirit(void* code, int spirit, int kind);

#ifdef __cplusplus
}
#endif
#endif
