#ifndef LOM_NATIVE_RESIDENT_H
#define LOM_NATIVE_RESIDENT_H

/**
 * @brief Load the game's executable (LOM_NATIVE_DISC_EXECUTABLE) from the disc
 *        and decode its globals, once.
 * @param disc_root The extracted disc's folder.
 * @return 0 when loaded (now or before), -1 if the file can't be read.
 */
int lom_native_load_resident(const char* disc_root);

#endif
