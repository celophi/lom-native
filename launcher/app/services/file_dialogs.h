#ifndef LOM_LAUNCHER_FILE_DIALOGS_H
#define LOM_LAUNCHER_FILE_DIALOGS_H

#include <QString>

class QWidget;

namespace lom::launcher {
class FileDialogs {
  public:
    /**
     * @brief Destroy it.
     */
    virtual ~FileDialogs() = default;

    /**
     * @brief Ask for the game's disc image.
     * @param parent The window the dialog belongs to.
     * @param start_directory Where it starts.
     * @return The file chosen, or empty when the player cancels.
     */
    virtual QString choose_disc_image(QWidget* parent, const QString& start_directory) = 0;

    /**
     * @brief Ask for a folder the game was extracted to.
     * @param parent The window the dialog belongs to.
     * @return The folder chosen, or empty when the player cancels.
     */
    virtual QString choose_game_folder(QWidget* parent) = 0;

    /**
     * @brief Ask where to keep the extracted games.
     * @param parent The window the dialog belongs to.
     * @param current The folder in use, where it starts.
     * @return The folder chosen, or empty when the player cancels.
     */
    virtual QString choose_games_root(QWidget* parent, const QString& current) = 0;

    /**
     * @brief Ask where to keep the memory cards.
     * @param parent The window the dialog belongs to.
     * @param current The folder in use, where it starts.
     * @return The folder chosen, or empty when the player cancels.
     */
    virtual QString choose_save_folder(QWidget* parent, const QString& current) = 0;

    /**
     * @brief Ask where the Remaster is installed.
     * @param parent The window the dialog belongs to.
     * @param current The folder in use, where it starts.
     * @return The folder chosen, or empty when the player cancels.
     */
    virtual QString choose_remaster_folder(QWidget* parent, const QString& current) = 0;

    /**
     * @brief Ask for a mod's folder, the one with its mod.json, to add.
     * @param parent The window the dialog belongs to.
     * @return The folder chosen, or empty when the player cancels.
     */
    virtual QString choose_mod_folder(QWidget* parent) = 0;

    /**
     * @brief Ask for a memory card file to import.
     * @param parent The window the dialog belongs to.
     * @return The file chosen, or empty when the player cancels.
     */
    virtual QString choose_memory_card(QWidget* parent) = 0;
};

}

#endif
