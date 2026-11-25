#ifndef LOM_SESSION_FOLDER_H
#define LOM_SESSION_FOLDER_H

#include <stddef.h>
#include <time.h>

/**
 * @brief Find where session folders go: LOM_SESSION_DIR, the portable folder's
 *        sessions/, or the platform's place for app state.
 * @param base Receives the folder.
 * @param size Bytes at @p base.
 * @return 0, or -1 when there's nowhere to put them.
 */
int lom_session_base_folder(char* base, size_t size);

/**
 * @brief Delete the oldest sessions so at most 20 are kept and the older ones
 *        add up to under 50 MB. The newest always stays.
 * @param base The sessions folder.
 */
void lom_session_prune(const char* base);

/**
 * @brief Copy the memory cards into the session's card/ folder, as they were
 *        when the run started.
 * @param folder The session's folder.
 * @return 0, or -1.
 */
int lom_session_snapshot_card(const char* folder);

/**
 * @brief Write the session's manifest.json: the build, when it started, and
 *        where the disc was.
 * @param folder The session's folder.
 * @param disc_root The extracted disc.
 * @param start When the session started.
 * @return 0, or -1.
 */
int lom_session_write_manifest(const char* folder, const char* disc_root, time_t start);

#endif
