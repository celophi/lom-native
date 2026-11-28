#ifndef LOM_REMASTER_INTERNAL_H
#define LOM_REMASTER_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Inflate a zlib stream.
     * @param in The stream.
     * @param in_size Bytes at @p in.
     * @param out Where to write what it inflates to.
     * @param out_size Bytes at @p out.
     * @param written Set to how many bytes it wrote.
     * @return 0, or -1 when the stream is bad or doesn't fit in @p out.
     */
    int lom_remaster_inflate(const unsigned char* in, size_t in_size, unsigned char* out, size_t out_size, size_t* written);

    /**
     * @brief Read a whole file, unpacking it if it's an `mdf` (a size and a zlib
     *        stream) as the Remaster's `.psb.m` files are.
     * @param path The file.
     * @param size Set to how many bytes it returns.
     * @return The bytes, for the caller to free, or NULL when the file can't be
     *         read or unpacked.
     */
    unsigned char* lom_remaster_load(const char* path, size_t* size);

    typedef struct
    {
        const unsigned char* data;
        size_t size;
        char** names;
        size_t name_count;
        size_t strings;
        size_t strings_data;
        size_t chunk_offsets, chunk_lengths, chunk_data;
        size_t root;
    } LomPsb;

    enum
    {
        LOM_PSB_NONE,
        LOM_PSB_BOOL,
        LOM_PSB_INT,
        LOM_PSB_FLOAT,
        LOM_PSB_INT_ARRAY,
        LOM_PSB_STRING,
        LOM_PSB_CHUNK,
        LOM_PSB_LIST,
        LOM_PSB_OBJECT,
        LOM_PSB_BAD
    };

    /**
     * @brief Start the thread that loads the Remaster's music, and have the SPU
     *        mix it in place of the PS1's songs (remaster_music.c).
     */
    void lom_remaster_music_start(void);

    /**
     * @brief Start the thread that loads the Remaster's sound effects, and have
     *        the SPU mix them in place of the PS1's (remaster_effects.c).
     */
    void lom_remaster_effects_start(void);

    /**
     * @brief Tell the sound effects about a file the game read, so they know
     *        which set of effects it has loaded.
     * @param disc_name Its path on the disc.
     * @param data Its bytes.
     * @param size Bytes at @p data.
     */
    void lom_remaster_effects_file(const char* disc_name, const unsigned char* data, size_t size);

    /**
     * @brief Put the Remaster's text, in the language chosen, into a scene file
     *        FIELD read (remaster_text_scenes.c).
     * @param disc_name Its path on the disc.
     * @param data Its bytes, changed in place.
     * @param size Its size.
     * @param room Bytes at @p data.
     * @return Its new size, or @p size when it's left alone.
     */
    int32_t lom_remaster_text_scene(const char* disc_name, unsigned char* data, int32_t size, size_t room);

    /**
     * @brief Register the GPU command that draws the Remaster's UI pictures
     *        (remaster_ui.c).
     */
    void lom_remaster_ui_initialize(void);

    /**
     * @brief Load one of the Remaster's pictures into the pictures' texture,
     *        if it isn't there already (remaster_ui.c).
     * @param file Its PSB file, under the Remaster's data folder.
     * @param label Its part's label, or NULL for the whole picture.
     * @param w Set to its width, if it can be drawn and this isn't NULL.
     * @param h Set to its height, the same way.
     * @return Nonzero when it can be drawn.
     */
    int lom_remaster_picture_ready(const char* file, const char* label, int* w, int* h);

    /**
     * @brief Get ready to draw part of one of the Remaster's pictures over
     *        four points on the screen (remaster_ui.c).
     * @param file Its PSB file.
     * @param label Its part's label, or NULL.
     * @param part The part of it, as x, y, width and height in its pixels, or
     *        NULL for all of it.
     * @param xy The points, x then y: top left, top right, bottom left, bottom
     *        right.
     * @param tpage The texture page word whose blend mode it uses, or -1 for
     *        the current one.
     * @param after A placement this one draws after, in the same command, or
     *        -1 for none.
     * @return The placement, for lom_remaster_picture_command, or -1 when it
     *         can't be drawn.
     */
    int lom_remaster_picture_place(const char* file, const char* label, const int part[4], const int xy[8], int tpage, int after);

    /**
     * @brief Turn a primitive the game built into a draw of a placement and
     *        what's chained after it, keeping its color and blending. The
     *        primitive's words past the fourth become no-ops.
     * @param code The primitive's code word.
     * @param words How many words it has from its code word on.
     * @param placement What lom_remaster_picture_place returned, or -1 to draw
     *        nothing.
     */
    void lom_remaster_picture_command(void* code, int words, int placement);

    /**
     * @brief Draw a placement with its own color or blending, instead of the
     *        primitive's it's drawn for (remaster_ui.c).
     * @param placement What lom_remaster_picture_place returned.
     * @param rgb The color, 0x808080 to draw it as it is, or -1 for the
     *        primitive's.
     * @param semi 1 to blend it the way the texture page says, 0 to draw it
     *        over what's there, or -1 for the primitive's choice.
     */
    void lom_remaster_picture_style(int placement, int rgb, int semi);

    /**
     * @brief Find where a 4-bit picture in video memory has anything drawn,
     *        from the copy kept of what the game uploaded (remaster_ui.c).
     * @param page_x The texture page's left, in video memory's 16-bit words.
     * @param page_y Its top.
     * @param u The picture's left on the page, in texels.
     * @param v Its top.
     * @param w Its width.
     * @param h Its height.
     * @param bounds Set to the part with texels that aren't color 0, as x, y,
     *        width and height from @p u and @p v.
     * @return 0, or -1 when it's all color 0 or there's no copy.
     */
    int lom_remaster_vram_bounds(int page_x, int page_y, int u, int v, int w, int h, int bounds[4]);

    typedef struct
    {
        char file[96];
        char label[32];
        int area[4];
        int xy[8];
    } LomRemasterPart;

    /**
     * @brief Where the Remaster's art for one sprite of one of WSEL's pictures
     *        comes from and goes (remaster_screens.c): each repainted picture
     *        the sprite covers, in the order they're drawn.
     * @param picture Which picture (WSEL_SPRITE_*).
     * @param x Where the picture's top left is on the screen.
     * @param y The same, down.
     * @param rect The sprite: x, y, width and height on the screen.
     * @param margin The wide frame's margin, or 0.
     * @param parts Set to the parts: the file, the picture's label, the part
     *        of it (x, y, width, height) and the four points it covers.
     * @param max Room in @p parts.
     * @return How many parts, 0 when the Remaster has no art for it.
     */
    int lom_remaster_wsel_parts(int picture, int x, int y, const int rect[4], int margin, LomRemasterPart parts[], int max);

    /**
     * @brief Where the Remaster's art for one quad of the title's backdrop
     *        comes from and goes (remaster_screens.c): its background, then its
     *        logo.
     * @param language The logo's language, a LOM_REMASTER_TEXT_* other than
     *        LOM_REMASTER_TEXT_PS1.
     * @param rect The quad: x, y, width and height on the screen.
     * @param margin The wide frame's margin, or 0.
     * @param parts Set to the parts, as lom_remaster_wsel_parts.
     * @param max Room in @p parts.
     * @return How many parts, 0 when the quad is off the backdrop.
     */
    int lom_remaster_title_parts(int language, const int rect[4], int margin, LomRemasterPart parts[], int max);

    /**
     * @brief Which of the Remaster's pictures draws one of name entry's
     *        glyphs, and where (remaster_screens.c): the whole picture, over
     *        the glyph's box moved to where the PS1's picture sits in it.
     * @param glyph The glyph.
     * @param rect The glyph's sprite: x, y, width and height on the screen.
     * @param part Set to the file, the picture's label and the four points;
     *        its area is unused.
     * @return 1 when the Remaster has a picture for the glyph, otherwise 0.
     */
    int lom_remaster_gname_part(int glyph, const int rect[4], LomRemasterPart* part);

    /**
     * @brief Where name entry's cursor points at a button or a tab in the
     *        Remaster's layout (remaster_screens.c): left of the button in the
     *        painted column, or the tab moved onto the wood.
     * @param position The cursor's position in name entry's table: 2 to 5 for
     *        the buttons, 6 to 9 for the tabs.
     * @param x The PS1's x, set to the Remaster layout's.
     * @param y The same, down.
     * @return 1 when the position is a button or a tab, otherwise 0, leaving
     *         them as they are.
     */
    int lom_remaster_gname_cursor_at(int position, int* x, int* y);

    /**
     * @brief Which of the Remaster's tab icons draws one of the menu's, and
     *        where (remaster_menu.c).
     * @param icon The menu's icon.
     * @param mirror Nonzero for the second column's first row.
     * @param rect The icon's sprite: x, y, width and height.
     * @param part Set to the file, the picture's label and the four points;
     *        its area is unused.
     * @return 1 when it's a tab icon the Remaster has, otherwise 0.
     */
    int lom_remaster_menu_icon_part(int icon, int mirror, const int rect[4], LomRemasterPart* part);

    /**
     * @brief Where the Remaster's art for the world map's day name comes from
     *        and goes (remaster_screens.c).
     * @param language The names' language, a LOM_REMASTER_TEXT_* other than
     *        LOM_REMASTER_TEXT_PS1.
     * @param u The PS1 sprite's texture position across.
     * @param v And down, which says the day.
     * @param rect The sprite: x, y, width and height on the screen.
     * @param part Set to the file, the part of its picture and the points.
     * @return 0, or -1 when there's no art for it.
     */
    int lom_remaster_day_name_part(int language, int u, int v, const int rect[4], LomRemasterPart* part);

    /**
     * @brief Where the Remaster's art for the world map's shell, or the day's
     *        spirit in it, comes from and goes (remaster_screens.c).
     * @param shell Nonzero for the shell, zero for the spirit.
     * @param u The PS1 sprite's texture position across.
     * @param v And down, which say the day.
     * @param rect The sprite: x, y, width and height on the screen.
     * @param part Set to the file, the picture's label, the part of it and the
     *        points.
     * @return 0, or -1 when there's no art for it.
     */
    int lom_remaster_kameo_part(int shell, int u, int v, const int rect[4], LomRemasterPart* part);

    /**
     * @brief The Remaster's banner file for a land's name on the world map
     *        (remaster_screens.c).
     * @param language The names' language, a LOM_REMASTER_TEXT_* other than
     *        LOM_REMASTER_TEXT_PS1.
     * @param selection The land, or 64 plus it for its artifact's name.
     * @param clut_row The PS1 name's palette row, which says whether the land
     *        is cleared.
     * @param file Set to the file.
     * @param size Room in @p file.
     * @return 0, or -1 when there's no banner for it.
     */
    int lom_remaster_land_name_file(int language, int selection, int clut_row, char* file, size_t size);

    /**
     * @brief Where a land name's banner goes: centered on the PS1's name, at
     *        the Remaster's 4.5 times (remaster_screens.c).
     * @param text Where the PS1's name has anything drawn: x, y, width and
     *        height on the screen.
     * @param w The banner's width, in its pixels.
     * @param h Its height.
     * @param xy Set to the four points it covers.
     */
    void lom_remaster_land_name_xy(const int text[4], int w, int h, int xy[8]);

    /**
     * @brief Where the Remaster's art for a world map spirit or its strength
     *        comes from and goes (remaster_screens.c).
     * @param spirit Which spirit, in the game's order.
     * @param kind LOM_REMASTER_WMAP_SPIRIT_*.
     * @param u The PS1 sprite's texture position across.
     * @param v And down, which say the frame or the strength.
     * @param rect The sprite: x, y, width and height on the screen.
     * @param part Set to the file, the picture's label, the part of it (width
     *        0 for all of it) and the points.
     * @param shade Set to how dark to draw it, 0x80 as it's painted.
     * @return 0, or -1 when there's no art for it.
     */
    int lom_remaster_spirit_part(int spirit, int kind, int u, int v, const int rect[4], LomRemasterPart* part, int* shade);

    /**
     * @brief Say whether the Remaster was found (remaster_settings.c).
     * @param root Its data folder, or NULL when it wasn't found.
     */
    void lom_remaster_set_found(const char* root);

    /**
     * @brief Start reading a PSB, the Remaster's tree of objects, lists,
     *        numbers, strings and chunks of bytes.
     * @param psb The reader.
     * @param data The PSB's bytes, which must outlive the reader.
     * @param size Bytes at @p data.
     * @return 0, or -1 when it isn't a PSB this reads.
     */
    int lom_psb_open(LomPsb* psb, const unsigned char* data, size_t size);

    /**
     * @brief Free what a reader holds. The data it read stays the caller's.
     * @param psb The reader.
     */
    void lom_psb_close(LomPsb* psb);

    /**
     * @brief Say what kind of value is somewhere in a PSB.
     * @param psb The reader.
     * @param value Its offset.
     * @return LOM_PSB_*, LOM_PSB_BAD when it's past the end or unknown.
     */
    int lom_psb_kind(const LomPsb* psb, size_t value);

    /**
     * @brief Find an object's member by name.
     * @param psb The reader.
     * @param object The object's offset.
     * @param name The member's name.
     * @param member Set to the member's offset.
     * @return 0, or -1 when @p object isn't an object or has no such member.
     */
    int lom_psb_member(const LomPsb* psb, size_t object, const char* name, size_t* member);

    /**
     * @brief Count an object's members.
     * @param psb The reader.
     * @param object The object's offset.
     * @return How many it has, 0 when it isn't an object.
     */
    size_t lom_psb_members(const LomPsb* psb, size_t object);

    /**
     * @brief Find an object's member by position.
     * @param psb The reader.
     * @param object The object's offset.
     * @param index Which member, from 0.
     * @param name Set to the member's name, which stays in the reader.
     * @param member Set to the member's offset.
     * @return 0, or -1 when @p object isn't an object or has no member there.
     */
    int lom_psb_member_at(const LomPsb* psb, size_t object, size_t index, const char** name, size_t* member);

    /**
     * @brief Count a list's items.
     * @param psb The reader.
     * @param list The list's offset.
     * @return How many it has, 0 when it isn't a list.
     */
    size_t lom_psb_count(const LomPsb* psb, size_t list);

    /**
     * @brief Find a list's item.
     * @param psb The reader.
     * @param list The list's offset.
     * @param index Which item, from 0.
     * @param item Set to the item's offset.
     * @return 0, or -1 when @p list isn't a list or has no item there.
     */
    int lom_psb_item(const LomPsb* psb, size_t list, size_t index, size_t* item);

    /**
     * @brief Read a number. A true is 1, and a float is cut to a whole number.
     * @param psb The reader.
     * @param value Its offset.
     * @return The number, 0 when it isn't one.
     */
    int64_t lom_psb_int(const LomPsb* psb, size_t value);

    /**
     * @brief Read a string.
     * @param psb The reader.
     * @param value Its offset.
     * @return The string, which stays in the PSB's data, or NULL when it isn't
     *         one.
     */
    const char* lom_psb_string(const LomPsb* psb, size_t value);

    /**
     * @brief Find a chunk of bytes, like a sound or a picture.
     * @param psb The reader.
     * @param value Its offset.
     * @param data Set to its bytes, which stay in the PSB's data.
     * @param size Set to how many there are.
     * @return 0, or -1 when it isn't a chunk.
     */
    int lom_psb_chunk(const LomPsb* psb, size_t value, const unsigned char** data, size_t* size);

    /**
     * @brief Read an item of an int array.
     * @param psb The reader.
     * @param array The array's offset.
     * @param index Which item, from 0.
     * @return The item, 0 when it isn't an int array or has no item there.
     */
    int64_t lom_psb_array_item(const LomPsb* psb, size_t array, size_t index);

    /**
     * @brief Follow a path of member names and list indexes, like
     *        "voice/music_voice/channelList/0".
     * @param psb The reader.
     * @param from Where the path starts, like @c psb->root.
     * @param path The path.
     * @param value Set to what's at the end of it.
     * @return 0, or -1 when something on the path isn't there.
     */
    int lom_psb_path(const LomPsb* psb, size_t from, const char* path, size_t* value);

#ifdef __cplusplus
}
#endif
#endif
