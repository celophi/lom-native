#ifndef LOM_NATIVE_CARD_H
#define LOM_NATIVE_CARD_H
#include <stdint.h>

typedef struct
{
    char name[20];

    int32_t attr, size;

    uint32_t next;

    int32_t head;

    char system[4];
} LomNativeCardEntry;

/**
 * @brief Keep the memory cards in a folder, with a slot0/ and a slot1/ folder
 *        in it for the two card slots. An empty slot folder is an empty card,
 *        and a missing one is no card.
 * @param path The folder, or NULL to go back to finding it the usual way
 *        (lom_native_card_root).
 * @return 0, or -1 while a card file is open, or for an empty or too long
 *         path.
 */
int lom_native_card_set_root(const char* path);

/**
 * @brief Find the folder the memory cards are in, creating it with a slot0/
 *        folder the first time: LOM_CARD_ROOT, then cards/ in the portable
 *        folder, then lom-native/cards in the user's data folder.
 * @return The folder, or NULL when none can be made.
 */
const char* lom_native_card_root(void);

/**
 * @brief Open a file on a card.
 * @param path "bu00:NAME" for slot 1 or "bu10:NAME" for slot 2, with a name of
 *        up to 20 characters.
 * @param flags 2 to write, and 0x200 to create it, with its size in 8192-byte
 *        blocks (1 to 15) in the top 16 bits.
 * @return A handle, 1 to 8, or -1 when it can't be opened, already exists
 *         when creating, or doesn't fit on the card.
 */
int lom_native_card_open(const char* path, int flags);

/**
 * @brief Close a card file.
 * @param handle The handle lom_native_card_open gave.
 * @return 0, or -1 for a handle that isn't open.
 */
int lom_native_card_close(int handle);

/**
 * @brief Read from a card file, from where the last read or write ended, and
 *        signal the card's I/O event, or its error event when it fails.
 * @param handle The file's handle.
 * @param data Where the bytes go.
 * @param size How many to read; the file has to have that many left.
 * @return @p size, or -1.
 */
int lom_native_card_read(int handle, void* data, int size);

/**
 * @brief Write to a card file and flush it to disk, from where the last read
 *        or write ended, and signal the card's I/O event, or its error event
 *        when it fails.
 * @param handle The file's handle, opened for writing.
 * @param data The bytes.
 * @param size How many to write; the file has to have room for them.
 * @return @p size, or -1.
 */
int lom_native_card_write(int handle, const void* data, int size);

/**
 * @brief Delete a card file.
 * @param path "bu00:NAME" or "bu10:NAME".
 * @return 1 when it's gone, 0 otherwise.
 */
int lom_native_card_erase(const char* path);

/**
 * @brief Rename a card file, on the same card.
 * @param from Its path, "bu00:NAME" or "bu10:NAME".
 * @param to Its new path, on the same card.
 * @return 1 when it's renamed, 0 otherwise.
 */
int lom_native_card_rename(const char* from, const char* to);

/**
 * @brief lom_native_card_open with the types the game's libapi.h uses, and
 *        reported to the saves boundary.
 * @param path The file's path.
 * @param mode The open flags.
 * @return A handle, or -1.
 */
long lom_native_card_open_libapi(char* path, unsigned long mode);

/**
 * @brief lom_native_card_close, with libapi.h's types, reported to the saves
 *        boundary.
 * @param handle The file's handle.
 * @return 0, or -1.
 */
long lom_native_card_close_libapi(long handle);

/**
 * @brief lom_native_card_read, with libapi.h's types, reported to the saves
 *        boundary.
 * @param handle The file's handle.
 * @param data Where the bytes go.
 * @param size How many to read.
 * @return @p size, or -1.
 */
long lom_native_card_read_libapi(long handle, void* data, long size);

/**
 * @brief lom_native_card_write, with libapi.h's types, reported to the saves
 *        boundary.
 * @param handle The file's handle.
 * @param data The bytes.
 * @param size How many to write.
 * @return @p size, or -1.
 */
long lom_native_card_write_libapi(long handle, void* data, long size);

/**
 * @brief lom_native_card_erase, with libapi.h's types, reported to the saves
 *        boundary.
 * @param path The file's path.
 * @return 1 when it's gone, 0 otherwise.
 */
long lom_native_card_erase_libapi(char* path);

/**
 * @brief lom_native_card_rename, with libapi.h's types, reported to the saves
 *        boundary.
 * @param from The file's path.
 * @param to Its new path.
 * @return 1 when it's renamed, 0 otherwise.
 */
long lom_native_card_rename_libapi(char* from, char* to);

/**
 * @brief Start listing a card's files, sorted by name and laid out in
 *        sectors from 64 the way a real card's would be.
 * @param pattern "bu00:*" or "bu10:*" for every file, or one file's path.
 * @param entry Filled in with the first file.
 * @return @p entry, or NULL when there's none or the card can't be read
 *         (which also signals the card's error event).
 */
void* lom_native_card_first(const char* pattern, LomNativeCardEntry* entry);

/**
 * @brief Go on to the next file of the listing lom_native_card_first
 *        started.
 * @param entry Filled in with the file.
 * @return @p entry, or NULL when there are no more.
 */
void* lom_native_card_next(LomNativeCardEntry* entry);

/**
 * @brief Check a card slot, signalling the card's software event with the
 *        result: I/O done for a card, new card for an empty slot, or error.
 * @param channel 0 for slot 1, 16 for slot 2.
 * @return 1.
 */
long lom_native_card_info(int channel);

/**
 * @brief The same check as lom_native_card_info, which is all loading a
 *        card's directory needs natively.
 * @param channel 0 for slot 1, 16 for slot 2.
 * @return 1.
 */
long lom_native_card_load(int channel);

/**
 * @brief The same check as lom_native_card_info, signalling the card's
 *        hardware event instead.
 * @param channel 0 for slot 1, 16 for slot 2.
 * @return 1.
 */
long lom_native_card_clear(int channel);

/**
 * @brief Wait for a card slot to finish what it was doing, which natively it
 *        always has.
 * @param slot 0 or 1.
 * @return 1 for a slot that exists, 0 otherwise.
 */
long lom_native_card_wait(int slot);

/**
 * @brief Read one 128-byte sector of a card, as lom_native_card_first lays
 *        its files out, and signal the card's hardware event.
 * @param channel 0 for slot 1, 16 for slot 2.
 * @param sector 64 to 1023.
 * @param data Where the 128 bytes go.
 * @return 1, or 0 for a sector no file covers or a read that fails.
 */
long lom_native_card_read_sector(int channel, int sector, void* data);

/**
 * @brief Write one 128-byte sector of a card, as lom_native_card_first lays
 *        its files out, and signal the card's hardware event.
 * @param channel 0 for slot 1, 16 for slot 2.
 * @param sector 64 to 1023.
 * @param data The 128 bytes.
 * @return 1, or 0 for a sector no file covers or a write that fails.
 */
long lom_native_card_write_sector(int channel, int sector, const void* data);

/**
 * @brief Refuse to format a card, signalling the card's error event. The
 *        port never wipes a player's saves.
 * @param channel 0 for slot 1, 16 for slot 2.
 * @return 0.
 */
long lom_native_card_format(int channel);

/**
 * @brief Open one of the card's events, the way the game's OpenEvent does.
 * @param desc 0xF4000001 for the software events, 0xF0000011 for the
 *        hardware ones.
 * @param spec Which result it waits for: 4 (I/O done), 0x8000 (error), 0x100
 *        (new card) or 0x2000 (timeout).
 * @param mode 0x2000; the port has no event callbacks.
 * @param callback 0.
 * @return The event's id, 1 to 8, or -1 for anything else or when all eight
 *         are open.
 */
long lom_native_card_open_event(unsigned int desc, int spec, int mode, unsigned int callback);

/**
 * @brief lom_native_card_open_event with the types the game's libapi.h uses.
 * @param desc The event's class.
 * @param spec Which result it waits for.
 * @param mode 0x2000.
 * @param callback NULL; a callback makes it fail.
 * @return The event's id, or -1.
 */
long lom_native_card_open_event_libapi(unsigned long desc, long spec, long mode, long (*callback)());

/**
 * @brief Close a card event.
 * @param event Its id.
 * @return 1, or 0 for an event that isn't open.
 */
long lom_native_card_close_event(int event);

/**
 * @brief Start a card event reporting.
 * @param event Its id.
 * @return 1, or 0 for an event that isn't open.
 */
long lom_native_card_enable_event(int event);

/**
 * @brief Check whether the card's latest result in an event's group was the
 *        one it waits for, and clear it.
 * @param event Its id.
 * @return 1 if it was, otherwise 0, and 0 for an event that isn't open or
 *         enabled.
 */
long lom_native_card_test_event(int event);

/**
 * @brief Find a Shift-JIS character's glyph for the memory card screens, in
 *        OpenBIOS's font, since there's no Kanji ROM.
 * @param code The character.
 * @return The address of its 30-byte glyph, or -1 when the font doesn't have
 *         it.
 */
#ifdef _WIN64
long long lom_native_card_krom(unsigned int code);
#else
long lom_native_card_krom(unsigned int code);
#endif

/**
 * @brief lom_native_card_first, filling in the game's own DIRENTRY.
 * @param pattern "bu00:*", "bu10:*" or one file's path.
 * @param entry The game's DIRENTRY, filled in with the first file.
 * @return 1, or 0 when there's none.
 */
long lom_native_niki_first(const char* pattern, void* entry);

/**
 * @brief lom_native_card_next, filling in the game's own DIRENTRY.
 * @param entry The game's DIRENTRY, filled in with the file.
 * @return 1, or 0 when there are no more.
 */
long lom_native_niki_next(void* entry);

/**
 * @brief Ask whether a card is a PocketStation, which natively it never is.
 * @param channel The card's channel.
 * @return 0, so the PocketStation check never starts.
 */
int32_t lom_native_niki_card_type(int32_t channel);

/**
 * @brief Ask whether FIELD's clock card is a PocketStation, which natively it
 *        never is.
 * @param port The card's port.
 * @return 0, so FIELD never reads a PocketStation's clock.
 */
int32_t lom_native_field_clock_card_type(int32_t port);

/**
 * @brief Add a card file name to the end of a path.
 * @param dst The path, with room for 20 more characters.
 * @param src The name; only its first 20 characters count.
 * @return 0.
 */
int32_t lom_native_niki_concat(char* dst, const char* src);

/**
 * @brief Copy a string, for the card code's sprintf-style calls that only
 *        ever copy one.
 * @param dst Where to copy it.
 * @param src The string.
 * @return 0.
 */
int32_t lom_native_niki_string_copy(char* dst, const char* src, ...);

/**
 * @brief Copy bytes in bcopy's argument order, for the save screens.
 * @param src Where to copy from.
 * @param dst Where to copy to.
 * @param count How many bytes.
 * @return @p dst.
 */
void* lom_native_save_copy(const void* src, void* dst, int32_t count);

/**
 * @brief Keep PSY-Z's interrupts out while the card code changes its state.
 */
void lom_native_card_lock(void);

/**
 * @brief Let PSY-Z's interrupts back in after lom_native_card_lock.
 */
void lom_native_card_unlock(void);

/**
 * @brief Stop the next card read or write from reporting a new card, as the
 *        SDK's _new_card does.
 */
void lom_native_card_new_card(void);

/**
 * @brief Stand in for the PocketStation library's McxSync, which FIELD's
 *        scripts can reach: stop FIELD, since the port has no PocketStation.
 * @param mode Unused.
 * @param command Unused.
 * @param result Unused.
 * @return Never returns.
 */
int lom_native_card_mcx_sync(int mode, long* command, long* result);

/**
 * @brief Stand in for McxGetTime: stop FIELD, since the port has no
 *        PocketStation.
 * @param port Unused.
 * @param time Unused.
 * @return Never returns.
 */
int lom_native_card_mcx_get_time(int port, unsigned char* time);

/**
 * @brief Stand in for McxHideTrans, which only CARDA's PocketStation save
 *        calls: stop the game.
 * @param channel Unused.
 * @return Never returns.
 */
int32_t lom_native_card_mcx_hide_trans(int32_t channel);

/**
 * @brief Stand in for McxExecFlag, which only CARDA's PocketStation save
 *        calls: stop the game.
 * @param channel Unused.
 * @param block Unused.
 * @param mode Unused.
 * @return Never returns.
 */
int32_t lom_native_card_mcx_exec_flag(int32_t channel, int32_t block, int32_t mode);
#endif
