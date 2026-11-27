#ifndef LOM_SETTINGS_PREFERENCE_STORE_H
#define LOM_SETTINGS_PREFERENCE_STORE_H

#include "preferences.h"
#include <string>

namespace lom::settings
{
class PreferenceStore
{
  public:
    enum class SaveReason
    {
        Changed,
        ExplicitRetry
    };

    /** Read the preference file. Missing or bad values get the defaults, and
     * duplicate control bindings are fixed. An empty path means nothing is
     * ever written. Keys from before the Remaster had a switch for each part
     * are read but no longer written: HD pictures turned off turns off every
     * picture's switch, and a soundtrack, sound effects or text chosen from
     * the Remaster turns its switch on. Their 0, the PS1's, was only the old
     * default, so it leaves the new switch on. */
    Preferences load(std::string path, const Preferences& defaults);

    /** Save, by writing a temporary file and renaming it over the old one.
     * Returns true if it saved, or nothing had changed. A Changed save that
     * already failed with these same values isn't tried again; an
     * ExplicitRetry is. */
    bool save(const Preferences& preferences, SaveReason reason);

    /** The values last saved. A failed save doesn't change them. */
    const Preferences& persisted() const;

    /** The preference file, or empty. */
    const std::string& path() const;

    /** Why the last save failed, or empty. */
    const std::string& error() const;

  private:
    std::string path_;

    std::string error_;

    Preferences persisted_;

    Preferences attempted_;

    bool attempted_save_ = false;
};

}
#endif
