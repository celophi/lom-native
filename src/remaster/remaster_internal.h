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
