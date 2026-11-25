#ifndef LOM_LAUNCHER_CARD_IMPORT_DIALOG_H
#define LOM_LAUNCHER_CARD_IMPORT_DIALOG_H

#include "lom/launcher/common/clock.h"
#include "lom/launcher/saves/memory_card.h"
#include "lom/launcher/common/random_source.h"
#include "lom/launcher/saves/save_importer.h"

#include <QDialog>

#include <filesystem>
#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTreeWidget;

namespace lom::launcher {

class ReleaseCatalog;

class CardImportDialog : public QDialog {
    Q_OBJECT

  public:
    /**
     * @brief Build the preview.
     * @param card Parsed memory card.
     * @param catalog Releases.
     * @param release_id Edition of the save folder.
     * @param save_root Save folder.
     * @param clock Time source for backup names; must outlive the dialog.
     * @param random Random source for backup names; must outlive the dialog.
     * @param parent Parent widget.
     */
    CardImportDialog(const MemoryCard& card, const ReleaseCatalog& catalog, std::string release_id, std::filesystem::path save_root, const Clock& clock,
                     RandomSource& random, QWidget* parent);

    /**
     * @brief Report how many Legend of Mana saves the card holds.
     * @return Count, including unusable ones.
     */
    int save_count() const;

    /**
     * @brief Report the import result after the dialog was accepted.
     */
    const SaveImporter::Result& result() const;

  private:
    /**
     * @brief Recompute row statuses for the selected slot.
     */
    void refresh();

    /**
     * @brief Import the selected saves.
     */
    void import_selected();

    const ReleaseCatalog& catalog_;
    const Clock& clock_;
    RandomSource& random_;
    std::string release_id_;
    std::filesystem::path save_root_;
    std::vector<MemoryCard::File> saves_;
    std::vector<bool> usable_;
    std::vector<bool> conflicting_;
    QTreeWidget* list_ = nullptr;
    QComboBox* slot_ = nullptr;
    QCheckBox* replace_ = nullptr;
    QPushButton* import_ = nullptr;
    QLabel* message_ = nullptr;
    SaveImporter::Result result_;
};

}

#endif
