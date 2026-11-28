#ifndef LOM_LAUNCHER_FILE_DIALOGS_PROVIDER_H
#define LOM_LAUNCHER_FILE_DIALOGS_PROVIDER_H

#include <QString>

class QWidget;

namespace lom::launcher {
class FileDialogsProvider {
  public:
    /**
     * @brief Destroy it.
     */
    virtual ~FileDialogsProvider() = default;

    /**
     * @brief Show the system's dialog for choosing a file, and wait for the player.
     * @param parent The window it belongs to.
     * @param title Its title.
     * @param start The folder it opens in.
     * @param filter The kinds of file it shows, in Qt's filter form.
     * @return The file chosen, or empty when the player cancelled.
     * @note The launcher's provider for the file dialogs: the one place it shows them, as thin as it can be. Which
     *       dialog, its words and where it starts are the launcher's (services/launcher_dialogs.h).
     */
    virtual QString choose_file(QWidget* parent, const QString& title, const QString& start, const QString& filter) = 0;

    /**
     * @brief Show the system's dialog for choosing a folder, and wait for the player.
     * @param parent The window it belongs to.
     * @param title Its title.
     * @param start The folder it opens in.
     * @return The folder chosen, or empty when the player cancelled.
     */
    virtual QString choose_folder(QWidget* parent, const QString& title, const QString& start) = 0;
};

}

#endif
