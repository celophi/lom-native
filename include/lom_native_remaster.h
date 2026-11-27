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

    enum
    {
        LOM_REMASTER_SWITCH_ALL,
        LOM_REMASTER_SWITCH_BACKGROUNDS,
        LOM_REMASTER_SWITCH_FACES,
        LOM_REMASTER_SWITCH_ENCYCLOPEDIA,
        LOM_REMASTER_SWITCH_TITLE,
        LOM_REMASTER_SWITCH_NAME_ENTRY,
        LOM_REMASTER_SWITCH_WORLD_SELECT,
        LOM_REMASTER_SWITCH_WORLD_MAP,
        LOM_REMASTER_SWITCH_MOVIES,
        LOM_REMASTER_SWITCH_MUSIC,
        LOM_REMASTER_SWITCH_ARRANGED,
        LOM_REMASTER_SWITCH_EFFECTS,
        LOM_REMASTER_SWITCH_TEXT,
        LOM_REMASTER_SWITCH_COUNT
    };

    /**
     * @brief Turn one of the Remaster's switches on or off. They're the ones
     *        F1's Remaster page shows: one that turns all of it on or off,
     *        then one for each part. All are on to start with, but Arranged,
     *        which picks the arranged soundtrack over the original recordings
     *        when Music is on. Each part takes effect from its next room,
     *        screen, song, effect or scene.
     * @param which LOM_REMASTER_SWITCH_*.
     * @param on Nonzero for on.
     */
    void lom_native_remaster_set_switch(int which, int on);

    /**
     * @brief Read one of the Remaster's switches as it's set, whatever the
     *        others are.
     * @param which LOM_REMASTER_SWITCH_*.
     * @return 1 when it's on.
     */
    int lom_native_remaster_switch(int which);

    /**
     * @brief Check whether a part of the Remaster is in use: it was found,
     *        and the part's switch and the one that turns all of it on are on
     *        (and Music, for Arranged).
     * @param which LOM_REMASTER_SWITCH_*.
     * @return 1 when it is.
     */
    int lom_native_remaster_uses(int which);

    /** @brief Nonzero when the renderer can draw the Remaster's backgrounds (src/gpu/hd_textures.h). */
    int lom_native_remaster_hd_supported(void);

    /** @brief Set the Backgrounds switch: draw rooms with the Remaster's backgrounds, from the next room on. */
    void lom_native_remaster_set_hd_backgrounds(int enabled);

    /** @brief Nonzero when rooms are drawn with the Remaster's backgrounds (lom_native_remaster_uses). */
    int lom_native_remaster_hd_backgrounds(void);

    /** @brief Set the Music and Arranged switches from where the music comes from (LOM_REMASTER_MUSIC_*), from the next song on. */
    void lom_native_remaster_set_music(int source);

    /** @brief Where the music comes from, with the switches as they are: LOM_REMASTER_MUSIC_*. */
    int lom_native_remaster_music(void);

    /** @brief Set the Effects switch: sound effects from the Remaster's recordings, from the next effect on. */
    void lom_native_remaster_set_effects(int enabled);

    /** @brief Nonzero when sound effects come from the Remaster's recordings (lom_native_remaster_uses). */
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

    /**
     * @brief Set the Text switch and its language from which text the scenes
     *        use (LOM_REMASTER_TEXT_*), from the next scene on: the PS1's
     *        turns the switch off and keeps the language.
     */
    void lom_native_remaster_set_text(int language);

    /** @brief Which text the scenes use, with the switches as they are: LOM_REMASTER_TEXT_*. */
    int lom_native_remaster_text(void);

    /** @brief Choose the Remaster's language for when the Text switch is on: a LOM_REMASTER_TEXT_* other than the PS1's. */
    void lom_native_remaster_set_text_language(int language);

    /** @brief The Remaster's language the Text switch uses: English to start with. */
    int lom_native_remaster_text_language(void);

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

    /**
     * @brief Set the switches of every repainted picture at once: faces,
     *        encyclopedia, title, name entry, world select and world map.
     */
    void lom_native_remaster_set_hd_ui(int enabled);

    /** @brief Nonzero when the renderer can show the Remaster's movies at its own resolution (src/gpu/hd_movie.h). */
    int lom_native_remaster_hd_movies_supported(void);

    /** @brief Set the Movies switch: play the Remaster's HD movies in place of the PS1's, from the next movie on. */
    void lom_native_remaster_set_hd_movies(int enabled);

    /** @brief Nonzero when the Remaster's HD movies play in place of the PS1's (lom_native_remaster_uses). */
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

    /**
     * @brief Draw the Remaster's title background and logo over the quads the
     *        game built for the title's backdrop (lom_draw_title_backdrop).
     * @param code The first quad's code word.
     * @param stride How far each next quad's is, in bytes.
     * @param count How many quads.
     */
    void lom_remaster_title_backdrop(void* code, int stride, int count);

    /** @brief Nonzero when the title's backdrop is the Remaster's, which is painted wide enough for a wide frame. */
    int lom_remaster_title_wide(void);

    /**
     * @brief Start a name entry: it's laid out as the Remaster's when HD
     *        pictures and Widescreen are on and its art can be drawn, since
     *        the Remaster's is painted for a wide frame. Before the screen sets
     *        up its cursor.
     * @return Nonzero when it's the Remaster's.
     */
    int lom_remaster_gname_begin(void);

    /** @brief End a name entry, back to the PS1's layout. */
    void lom_remaster_gname_end(void);

    /** @brief Nonzero while a name entry is laid out as the Remaster's. */
    int lom_remaster_gname_layout(void);

    /**
     * @brief Draw the Remaster's background over name entry's fixed sprites
     *        (lom_draw_gname_background).
     * @param code The first sprite's code word.
     * @param stride How far each next sprite's is, in bytes.
     * @param count How many sprites.
     */
    void lom_remaster_gname_background(void* code, int stride, int count);

    /**
     * @brief Lay out one of name entry's glyphs as the Remaster does
     *        (lom_draw_gname_glyph): drop a button the background has painted
     *        in, or move the delete button's arrow onto its painted button,
     *        move a tab onto the wood, or draw the Remaster's picture of it.
     * @param code The glyph's sprite's code word.
     * @param sprites How many sprites it is: 2 with its shadow, else 1.
     * @param stride How far the shadow's code word is, in bytes.
     * @param glyph The glyph.
     */
    void lom_remaster_gname_glyph(void* code, int sprites, int stride, int glyph);

    /** @brief How far name entry's tabs move across in the Remaster's layout, 0 in the PS1's. */
    int lom_remaster_gname_tab_shift(void);

    /**
     * @brief Move name entry's cursor target to a button or tab's place in the
     *        Remaster's layout, when it's in use (lom_draw_gname_cursor).
     * @param position The target's position in the screen's table.
     * @param x Its x, moved.
     * @param y Its y, moved.
     */
    void lom_remaster_gname_cursor(int position, int* x, int* y);

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
