#ifndef CARD_SLOT_H
#define CARD_SLOT_H

#include <dirent.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

/**
 * @brief Open a slot's folder, which is the card.
 * @param folder The folder.
 * @param slot 0 or 1.
 * @return A handle to the folder (on Windows, just the slot again), or -1.
 */
int card_slot_open(const char* folder, int slot);

/**
 * @brief Close what card_slot_open opened.
 * @param dir Its handle.
 */
void card_slot_close(int dir);

/**
 * @brief List a slot's files.
 * @param dir The slot's handle, which the listing takes over on Linux and macOS.
 * @return The listing, or NULL.
 */
DIR* card_slot_list(int dir);

/**
 * @brief Open a file on a card, refusing symbolic links on Linux and macOS.
 * @param dir The slot's handle.
 * @param name The file's name.
 * @param flags open's flags.
 * @return The file, or -1.
 */
int card_slot_open_file(int dir, const char* name, int flags);

/**
 * @brief Read a card file's details, without following symbolic links.
 * @param dir The slot's handle.
 * @param name The file's name.
 * @param st Filled in.
 * @return 0, or -1.
 */
int card_slot_stat(int dir, const char* name, struct stat* st);

/**
 * @brief Delete a card file.
 * @param dir The slot's handle.
 * @param name The file's name.
 * @return 0, or -1.
 */
int card_slot_unlink(int dir, const char* name);

/**
 * @brief Rename a card file, refusing to replace one that already has the new
 *        name.
 * @param dir The slot's handle.
 * @param from Its name.
 * @param to Its new name.
 * @return 0, or -1.
 */
int card_slot_rename(int dir, const char* from, const char* to);

/**
 * @brief Flush a slot's folder to disk, so a new or renamed file survives a
 *        crash. Nothing to do on Windows.
 * @param dir The slot's handle.
 * @return 0, or -1.
 */
int card_slot_sync(int dir);

/**
 * @brief Flush a card file to disk.
 * @param fd The file.
 * @return 0, or -1.
 */
int card_file_sync(int fd);

/**
 * @brief Read or write bytes at an offset in a card file.
 * @param fd The file.
 * @param data The bytes.
 * @param size How many.
 * @param offset Where in the file.
 * @param writing Nonzero to write.
 * @return How many were moved, or -1.
 */
ssize_t card_file_transfer(int fd, void* data, size_t size, off_t offset, int writing);

#endif
