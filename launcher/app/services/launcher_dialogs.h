#ifndef LOM_LAUNCHER_LAUNCHER_DIALOGS_H
#define LOM_LAUNCHER_LAUNCHER_DIALOGS_H

#include "services/file_dialogs_provider.h"

#include <QString>

class QWidget;

namespace lom::launcher {
class LauncherDialogs {
  public:
    /**
     * @brief The launcher's file dialogs, shown through a provider.
     * @param provider What shows them.
     */
    explicit LauncherDialogs(FileDialogsProvider& provider);

    /**
     * @brief Ask for a disc image.
     * @param parent The window.
     * @param start_directory The folder to start in.
     * @return The image, or empty.
     */
    QString choose_disc_image(QWidget* parent, const QString& start_directory);

    /**
     * @brief Ask for the folder an extracted game is in, starting at the player's home.
     * @param parent The window.
     * @return The folder, or empty.
     */
    QString choose_game_folder(QWidget* parent);

    /**
     * @brief Ask for the folder disc images are copied to.
     * @param parent The window.
     * @param current The folder now.
     * @return The folder, or empty.
     */
    QString choose_games_root(QWidget* parent, const QString& current);

    /**
     * @brief Ask for the save folder.
     * @param parent The window.
     * @param current The folder now.
     * @return The folder, or empty.
     */
    QString choose_save_folder(QWidget* parent, const QString& current);

    /**
     * @brief Ask for the Remaster's folder, starting at the one now, or at the player's home when there's none.
     * @param parent The window.
     * @param current The folder now, or empty.
     * @return The folder, or empty.
     */
    QString choose_remaster_folder(QWidget* parent, const QString& current);

    /**
     * @brief Ask for a mod's folder, starting at the player's home.
     * @param parent The window.
     * @return The folder, or empty.
     */
    QString choose_mod_folder(QWidget* parent);

    /**
     * @brief Ask for an emulator memory card, starting at the player's home.
     * @param parent The window.
     * @return The card, or empty.
     */
    QString choose_memory_card(QWidget* parent);

  private:
    FileDialogsProvider& provider_;
};

}

#endif
