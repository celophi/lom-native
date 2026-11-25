#include "dialogs/card_import_dialog.h"

#include "lom/launcher/saves/lom_save.h"
#include "lom/launcher/disc/release_catalog.h"
#include "lom/launcher/saves/save_folder.h"
#include "window/theme.h"
#include "window/ui_text.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <set>

namespace lom::launcher {

namespace {

constexpr int dialog_width = 560;
constexpr int column_name = 0;
constexpr int column_edition = 1;
constexpr int column_status = 2;

}

CardImportDialog::CardImportDialog(const MemoryCard& card, const ReleaseCatalog& catalog, std::string release_id, std::filesystem::path save_root,
                                   const Clock& clock, RandomSource& random, QWidget* parent)
    : QDialog(parent), catalog_(catalog), clock_(clock), random_(random), release_id_(std::move(release_id)), save_root_(std::move(save_root)) {
    setWindowTitle(ui_text::card_import_title);
    setMinimumWidth(dialog_width);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(ui_text::card_import_intro, this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    for (const MemoryCard::File& file : card.files()) {
        if (catalog_.by_card_filename(file.name) != nullptr) {
            saves_.push_back(file);
        }
    }

    list_ = new QTreeWidget(this);
    list_->setHeaderLabels({ui_text::card_column_name, ui_text::card_column_edition, ui_text::card_column_status});
    list_->setRootIsDecorated(false);
    list_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(list_);

    auto* slot_row = new QHBoxLayout();
    slot_row->addWidget(new QLabel(ui_text::card_slot_label, this));
    slot_ = new QComboBox(this);

    for (int slot = 0; slot < SaveFolder::slot_count; slot++) {
        slot_->addItem(QString(ui_text::card_slot_name).arg(slot + 1), slot);
    }

    slot_row->addWidget(slot_, 1);
    layout->addLayout(slot_row);
    replace_ = new QCheckBox(ui_text::card_replace, this);
    layout->addWidget(replace_);
    message_ = new QLabel(this);
    Theme::set_role(message_, Theme::muted_label);
    message_->setWordWrap(true);
    layout->addWidget(message_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(ui_text::cancel);
    import_ = buttons->addButton(ui_text::card_import_button, QDialogButtonBox::AcceptRole);
    Theme::set_role(import_, Theme::primary_button);
    layout->addWidget(buttons);

    connect(slot_, &QComboBox::currentIndexChanged, this, &CardImportDialog::refresh);
    connect(replace_, &QCheckBox::toggled, this, [this](bool replace) {
        for (std::size_t i = 0; i < saves_.size(); i++) {
            if (conflicting_[i]) {
                list_->topLevelItem(int(i))->setCheckState(column_name, replace ? Qt::Checked : Qt::Unchecked);
            }
        }
    });
    connect(list_, &QTreeWidget::itemChanged, this, [this]() {
        import_->setEnabled(true);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(import_, &QPushButton::clicked, this, &CardImportDialog::import_selected);
    refresh();
}

int CardImportDialog::save_count() const {
    return int(saves_.size());
}

const SaveImporter::Result& CardImportDialog::result() const {
    return result_;
}

void CardImportDialog::refresh() {
    const int slot = slot_->currentData().toInt();
    std::vector<MemoryCard::File> valid;

    for (const MemoryCard::File& file : saves_) {
        if (file.problem.empty()) {
            valid.push_back(file);
        }
    }

    const SaveImporter::Plan plan = SaveImporter::plan(save_root_, slot, valid);
    const auto contains = [](const std::vector<std::string>& names, const std::string& name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    };

    list_->blockSignals(true);
    list_->clear();
    usable_.assign(saves_.size(), false);
    conflicting_.assign(saves_.size(), false);

    for (std::size_t i = 0; i < saves_.size(); i++) {
        const MemoryCard::File& file = saves_[i];
        const ReleaseCatalog::Release* release = catalog_.by_card_filename(file.name);
        auto* item = new QTreeWidgetItem(list_);
        item->setText(column_name, QString::fromStdString(file.name));
        item->setText(column_edition, QString::fromStdString(release->region));
        const bool valid_save = file.problem.empty() && LomSave::validate(file.bytes.data(), file.bytes.size()) == LomSave::Status::Valid;
        QString status;

        if (!valid_save) {
            status = ui_text::card_status_corrupt;
        } else if (release->id != release_id_) {
            status = ui_text::card_status_other_region;
        } else if (contains(plan.identical_names, file.name)) {
            status = ui_text::card_status_identical;
        } else if (contains(plan.conflicting_names, file.name)) {
            status = ui_text::card_status_conflict;
            usable_[i] = true;
            conflicting_[i] = true;
        } else {
            status = ui_text::card_status_ready;
            usable_[i] = true;
        }

        item->setText(column_status, status);
        Qt::ItemFlags flags = Qt::ItemIsEnabled;

        if (usable_[i]) {
            flags |= Qt::ItemIsUserCheckable | Qt::ItemIsSelectable;
        }

        item->setFlags(flags);
        const bool selected = usable_[i] && (!conflicting_[i] || replace_->isChecked());
        item->setCheckState(column_name, selected ? Qt::Checked : Qt::Unchecked);
    }

    list_->blockSignals(false);
    replace_->setVisible(!plan.conflicting_names.empty());
    const bool any = std::find(usable_.begin(), usable_.end(), true) != usable_.end();
    import_->setEnabled(any);
    message_->setText(saves_.empty() ? ui_text::card_no_saves : QString());
}

void CardImportDialog::import_selected() {
    const int slot = slot_->currentData().toInt();
    std::vector<MemoryCard::File> selected;
    std::set<std::string> replace;

    for (std::size_t i = 0; i < saves_.size(); i++) {
        if (usable_[i] && list_->topLevelItem(int(i))->checkState(column_name) == Qt::Checked) {
            selected.push_back(saves_[i]);

            if (replace_->isChecked()) {
                replace.insert(saves_[i].name);
            }
        }
    }

    result_ = SaveImporter::commit(save_root_, slot, selected, replace, catalog_, release_id_, clock_, random_);

    if (result_.status == SaveImporter::Result::Status::Imported || result_.status == SaveImporter::Result::Status::NothingToImport) {
        accept();
        return;
    }

    refresh();
    message_->setText(QString::fromStdString(result_.error));
}

}
