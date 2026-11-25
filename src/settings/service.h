#ifndef LOM_SETTINGS_SERVICE_H
#define LOM_SETTINGS_SERVICE_H

#include "preference_store.h"

namespace lom::settings
{
class SettingsService
{
  public:
    /**
     * @brief Load the preferences and apply them, once. Call it on the SDL
     *        thread.
     * @param path The preferences file, or NULL for the usual settings folder.
     */
    void enable(const char* path);

    /**
     * @brief Say whether the preferences have been loaded.
     * @return Whether they have.
     */
    bool enabled() const;

    /**
     * @brief The preferences, to read or change.
     * @return The preferences.
     */
    Preferences& values();

    /**
     * @brief The preferences, to read.
     * @return The preferences.
     */
    const Preferences& values() const;

    /**
     * @brief Where the preferences are saved, and how the last save went.
     * @return The store.
     */
    const PreferenceStore& store() const;

    /**
     * @brief Save the preferences if they changed. Call it every frame. A save
     *        that failed isn't retried.
     */
    void update();

    /**
     * @brief Save the preferences now, retrying a save that failed.
     */
    void flush();

  private:
    /**
     * @brief Work out what to save: the preferences, with the display settings
     *        the display has, or the last saved ones while a display change
     *        is waiting.
     * @return What to save.
     */
    Preferences persistence_snapshot();

    Preferences values_;

    PreferenceStore store_;

    bool enabled_ = false;
};

/**
 * @brief The game's settings. Use them on the SDL thread.
 * @return The settings.
 */
SettingsService& settings();

}
#endif
