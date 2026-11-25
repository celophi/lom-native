#include "dialogs/mods_dialog.h"
#include "services/desktop.h"
#include "services/file_dialogs.h"
#include "window/app_paths.h"
#include "window/theme.h"
#include "window/ui_text.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <system_error>
#include <utility>

namespace lom::launcher {

namespace {

constexpr int dialog_width = 520;
constexpr int list_height = 220;
constexpr int id_role = Qt::UserRole;

/**
 * @brief Join names for a sentence, with commas.
 * @param names The names.
 * @return "a, b, c".
 */
QString joined(const std::vector<std::string>& names) {
    QStringList parts;

    for (const std::string& name : names) {
        parts.append(QString::fromStdString(name));
    }

    return parts.join(QStringLiteral(", "));
}

/**
 * @brief Make a list row's text: the mod's name and version.
 * @param entry The mod.
 * @return The text.
 */
QString row_text(const ModEntry& entry) {
    const QString name = QString::fromStdString(entry.name);
    const QString text = entry.version.empty() ? name : name + QStringLiteral("  ") + QString::fromStdString(entry.version);

    if (!entry.clashes.empty()) {
        return text + QStringLiteral("  ") + QString(ui_text::mod_row_clash);
    }

    if (!entry.unmet.empty()) {
        return text + QStringLiteral("  ") + QString(ui_text::mod_row_unmet);
    }

    return text;
}

}

ModsDialog::ModsDialog(ModLibrary library, FileDialogs& dialogs, Desktop& desktop, QWidget* parent)
    : QDialog(parent), library_(std::move(library)), dialogs_(dialogs), desktop_(desktop) {
    setWindowTitle(ui_text::mods_title);
    setMinimumWidth(dialog_width);
    auto* layout = new QVBoxLayout(this);

    auto* note = new QLabel(ui_text::mods_note, this);
    note->setWordWrap(true);
    Theme::set_role(note, Theme::muted_label);
    layout->addWidget(note);

    auto* body = new QHBoxLayout();
    list_ = new QListWidget(this);
    list_->setObjectName(list_name);
    list_->setMinimumHeight(list_height);
    body->addWidget(list_, 1);

    auto* order = new QVBoxLayout();
    up_ = new QPushButton(ui_text::mod_up, this);
    up_->setObjectName(up_name);
    down_ = new QPushButton(ui_text::mod_down, this);
    down_->setObjectName(down_name);
    order->addWidget(up_);
    order->addWidget(down_);
    order->addStretch();
    body->addLayout(order);
    layout->addLayout(body);

    empty_ = new QLabel(ui_text::mods_empty, this);
    empty_->setWordWrap(true);
    Theme::set_role(empty_, Theme::muted_label);
    layout->addWidget(empty_);

    details_ = new QLabel(this);
    details_->setObjectName(details_name);
    details_->setWordWrap(true);
    details_->setTextFormat(Qt::RichText);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(details_);

    status_ = new QLabel(this);
    status_->setObjectName(status_name);
    status_->setWordWrap(true);
    status_->setVisible(false);
    layout->addWidget(status_);

    auto* actions = new QHBoxLayout();
    auto* add = new QPushButton(ui_text::mod_add, this);
    add->setObjectName(add_name);
    remove_ = new QPushButton(ui_text::mod_remove, this);
    remove_->setObjectName(remove_name);
    auto* open_folder = new QPushButton(ui_text::mod_open_folder, this);
    open_folder->setObjectName(open_folder_name);
    actions->addWidget(add);
    actions->addWidget(remove_);
    actions->addStretch();
    actions->addWidget(open_folder);
    layout->addLayout(actions);

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(ui_text::close, QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(list_, &QListWidget::currentRowChanged, this, [this]() {
        show_details();
    });
    connect(list_, &QListWidget::itemChanged, this, &ModsDialog::toggled);
    connect(up_, &QPushButton::clicked, this, [this]() {
        move(-1);
    });
    connect(down_, &QPushButton::clicked, this, [this]() {
        move(1);
    });
    connect(add, &QPushButton::clicked, this, &ModsDialog::add_mod);
    connect(remove_, &QPushButton::clicked, this, &ModsDialog::remove_mod);
    connect(open_folder, &QPushButton::clicked, this, [this]() {
        std::error_code ignored;
        std::filesystem::create_directories(library_.folder(), ignored);
        desktop_.open_folder(AppPaths::to_text(library_.folder()));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    reload(std::string());
}

void ModsDialog::reload(const std::string& select_id) {
    entries_ = library_.scan();
    int select_row = entries_.empty() ? -1 : 0;

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].id == select_id) {
            select_row = static_cast<int>(i);
        }
    }

    fill_list(select_row);
}

void ModsDialog::fill_list(int select_row) {
    const QSignalBlocker blocker(list_);
    list_->clear();

    for (const ModEntry& entry : entries_) {
        auto* item = new QListWidgetItem(row_text(entry), list_);
        item->setData(id_role, QString::fromStdString(entry.id));

        if (entry.refused.empty()) {
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(entry.enabled ? Qt::Checked : Qt::Unchecked);
        } else {
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            item->setData(Qt::CheckStateRole, Qt::Unchecked);
            item->setForeground(list_->palette().brush(QPalette::Disabled, QPalette::Text));
        }
    }

    empty_->setVisible(entries_.empty());
    list_->setCurrentRow(select_row);
    show_details();
}

void ModsDialog::show_details() {
    const int row = selected_row();
    remove_->setEnabled(row >= 0 && !entries_[row].bundled);
    up_->setEnabled(row > 0);
    down_->setEnabled(row >= 0 && row + 1 < static_cast<int>(entries_.size()));

    if (row < 0) {
        details_->clear();
        return;
    }

    const ModEntry& entry = entries_[row];
    QString text = QStringLiteral("<b>%1</b>").arg(QString::fromStdString(entry.name).toHtmlEscaped());

    if (!entry.version.empty()) {
        text += QStringLiteral(" ") + QString::fromStdString(entry.version).toHtmlEscaped();
    }

    if (!entry.author.empty()) {
        text += QStringLiteral(", ") + QString(ui_text::mod_by).arg(QString::fromStdString(entry.author)).toHtmlEscaped();
    }

    QStringList lines{text};

    if (!entry.description.empty()) {
        lines.append(QString::fromStdString(entry.description).toHtmlEscaped());
    }

    if (entry.bundled) {
        lines.append(QString(ui_text::mod_example).toHtmlEscaped());
    }

    if (!entry.refused.empty()) {
        lines.append(QString(ui_text::mod_refused).arg(QString::fromStdString(entry.refused)).toHtmlEscaped());
    }

    if (!entry.clashes.empty()) {
        lines.append(QStringLiteral("<b>%1</b>").arg(QString(ui_text::mod_clashes).arg(joined(entry.clashes)).toHtmlEscaped()));
    }

    if (!entry.unmet.empty()) {
        lines.append(QString(ui_text::mod_unmet).arg(QString::fromStdString(entry.unmet)).toHtmlEscaped());
    }

    for (const std::string& warning : entry.warnings) {
        lines.append(QString(ui_text::mod_warning).arg(QString::fromStdString(warning)).toHtmlEscaped());
    }

    if (entry.changes_gameplay) {
        lines.append(QString(ui_text::mod_changes_gameplay).toHtmlEscaped());
    }

    if (entry.manifest && entry.manifest->in_game_toggle) {
        lines.append(QString(ui_text::mod_in_game_toggle).toHtmlEscaped());
    }

    if (!entry.switches.empty()) {
        lines.append(QString(ui_text::mod_sets_switches).arg(joined(entry.switches)).toHtmlEscaped());
    }

    if (!entry.overridden_switches.empty()) {
        lines.append(QString(ui_text::mod_overridden).arg(joined(entry.overridden_switches)).toHtmlEscaped());
    }

    if (!entry.overridden_by.empty()) {
        lines.append(QString(ui_text::mod_overrides).arg(joined(entry.overridden_by)).toHtmlEscaped());
    }

    details_->setText(lines.join(QStringLiteral("<br>")));
}

bool ModsDialog::save() {
    std::string error;
    ModLibrary::resolve_conflicts(entries_);

    if (!library_.save(entries_, error)) {
        status_->setText(QString(ui_text::mod_save_failed).arg(QString::fromStdString(error)));
        status_->setVisible(true);
        return false;
    }

    status_->setVisible(false);
    return true;
}

void ModsDialog::toggled(QListWidgetItem* item) {
    const int row = list_->row(item);

    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        return;
    }

    entries_[row].enabled = item->checkState() == Qt::Checked;
    save();
    fill_list(row);
}

void ModsDialog::move(int step) {
    const int row = selected_row();
    const int target = row + step;

    if (row < 0 || target < 0 || target >= static_cast<int>(entries_.size())) {
        return;
    }

    std::swap(entries_[row], entries_[target]);
    save();
    fill_list(target);
}

void ModsDialog::add_mod() {
    const QString chosen = dialogs_.choose_mod_folder(this);

    if (chosen.isEmpty()) {
        return;
    }

    const std::filesystem::path source = AppPaths::to_path(chosen);
    std::string error;
    const std::optional<std::string> id = ModLibrary::source_id(source, error);
    bool replace = false;

    if (id && library_.installed(*id)) {
        const QString name = QString::fromStdString(*id);
        replace = QMessageBox::question(this, QString(ui_text::mod_replace_title).arg(name), ui_text::mod_replace_body) == QMessageBox::Yes;

        if (!replace) {
            return;
        }
    }

    const std::optional<std::string> added = id ? library_.add(source, replace, error) : std::nullopt;

    if (!added) {
        status_->setText(QString(ui_text::mod_add_failed).arg(QString::fromStdString(error)));
        status_->setVisible(true);
        return;
    }

    status_->setVisible(false);
    reload(*added);
}

void ModsDialog::remove_mod() {
    const int row = selected_row();

    if (row < 0) {
        return;
    }

    const std::string id = entries_[row].id;
    const QString name = QString::fromStdString(entries_[row].name);

    if (QMessageBox::question(this, QString(ui_text::mod_remove_title).arg(name), ui_text::mod_remove_body) != QMessageBox::Yes) {
        return;
    }

    std::string error;

    if (!library_.remove(id, error)) {
        status_->setText(QString(ui_text::mod_remove_failed).arg(QString::fromStdString(error)));
        status_->setVisible(true);
        return;
    }

    entries_.erase(entries_.begin() + row);

    if (save()) {
        reload(std::string());
    }
}

int ModsDialog::selected_row() const {
    const int row = list_->currentRow();
    return row >= 0 && row < static_cast<int>(entries_.size()) ? row : -1;
}

}
