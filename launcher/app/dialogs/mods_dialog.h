#ifndef LOM_LAUNCHER_MODS_DIALOG_H
#define LOM_LAUNCHER_MODS_DIALOG_H

#include "lom/launcher/mods/mod_library.h"

#include <QDialog>

#include <string>
#include <vector>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace lom::launcher {

class Desktop;
class FileDialogs;

class ModsDialog : public QDialog {
    Q_OBJECT

  public:
    static constexpr char list_name[] = "modList";
    static constexpr char details_name[] = "modDetails";
    static constexpr char status_name[] = "modsStatus";
    static constexpr char add_name[] = "addModButton";
    static constexpr char remove_name[] = "removeModButton";
    static constexpr char up_name[] = "moveModUpButton";
    static constexpr char down_name[] = "moveModDownButton";
    static constexpr char open_folder_name[] = "openModsFolderButton";

    /**
     * @brief Show the mods in a folder, to turn on and off, order, add and
     *        remove. Every change is saved to the folder's mods.ini right away.
     * @param library The mods folder.
     * @param dialogs Asks for a mod's folder to add.
     * @param desktop Opens the mods folder.
     * @param parent The launcher's window.
     */
    ModsDialog(ModLibrary library, FileDialogs& dialogs, Desktop& desktop, QWidget* parent);

  private:
    /**
     * @brief Read the mods folder again and show it, keeping a mod selected.
     * @param select_id The mod to select; empty selects the first.
     */
    void reload(const std::string& select_id);

    /**
     * @brief Fill the list from the entries. A mod the game would load has a
     *        checkbox; one it would refuse has an empty one that can't be
     *        ticked, and is dimmed.
     * @param select_row The row to select, or -1 for none.
     */
    void fill_list(int select_row);

    /**
     * @brief Describe the selected mod below the list, and enable the buttons
     *        that apply to it.
     */
    void show_details();

    /**
     * @brief Save which mods are on, in order, and work out their conflicts
     *        again.
     * @return True when it was saved; otherwise the status says why.
     */
    bool save();

    /**
     * @brief Turn a mod on or off when its checkbox changes.
     * @param item The mod's row.
     */
    void toggled(QListWidgetItem* item);

    /**
     * @brief Move the selected mod up or down the list.
     * @param step -1 for up, 1 for down.
     */
    void move(int step);

    /**
     * @brief Ask for a mod's folder and copy it in, asking first before it
     *        replaces a mod with the same id.
     */
    void add_mod();

    /**
     * @brief Delete the selected mod's folder, after asking.
     */
    void remove_mod();

    /**
     * @brief Find the selected row.
     * @return Its index, or -1 when nothing is selected.
     */
    int selected_row() const;

    ModLibrary library_;
    FileDialogs& dialogs_;
    Desktop& desktop_;
    std::vector<ModEntry> entries_;
    QListWidget* list_ = nullptr;
    QLabel* empty_ = nullptr;
    QLabel* details_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* remove_ = nullptr;
    QPushButton* up_ = nullptr;
    QPushButton* down_ = nullptr;
};

}

#endif
