#ifndef LOM_LAUNCHER_NATIVE_FILE_DIALOGS_H
#define LOM_LAUNCHER_NATIVE_FILE_DIALOGS_H

#include "services/file_dialogs.h"

namespace lom::launcher {
class NativeFileDialogs : public FileDialogs {
  public:
    /**
     * @brief Ask for a disc image with the native picker.
     * @param parent Dialog parent.
     * @param start_directory Folder to open first.
     * @return The chosen file, or empty when cancelled.
     */
    QString choose_disc_image(QWidget* parent, const QString& start_directory) override;

    /**
     * @brief Ask for an extracted game folder with the native picker.
     * @param parent Dialog parent.
     * @return The chosen folder, or empty when cancelled.
     */
    QString choose_game_folder(QWidget* parent) override;

    /**
     * @brief Ask for the folder for new game files with the native picker.
     * @param parent Dialog parent.
     * @param current Current folder.
     * @return The chosen folder, or empty when cancelled.
     */
    QString choose_games_root(QWidget* parent, const QString& current) override;

    /**
     * @brief Ask for a save folder with the native picker.
     * @param parent Dialog parent.
     * @param current Current folder.
     * @return The chosen folder, or empty when cancelled.
     */
    QString choose_save_folder(QWidget* parent, const QString& current) override;

    /**
     * @brief Ask for the Remaster's folder with the platform picker.
     * @param parent Dialog parent.
     * @param current Folder to open first.
     * @return The chosen folder, or empty when cancelled.
     */
    QString choose_remaster_folder(QWidget* parent, const QString& current) override;

    /**
     * @brief Ask for a mod's folder with the platform picker.
     * @param parent Dialog parent.
     * @return The chosen folder, or empty when cancelled.
     */
    QString choose_mod_folder(QWidget* parent) override;

    /**
     * @brief Ask for an emulator memory card with the native picker.
     * @param parent Dialog parent.
     * @return The chosen file, or empty when cancelled.
     */
    QString choose_memory_card(QWidget* parent) override;
};

}

#endif
