#include "dialogs/game_files_dialog.h"

#include "window/theme.h"
#include "window/ui_text.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace lom::launcher {

namespace {

constexpr int dialog_width = 540;
constexpr int section_spacing = 14;
constexpr int option_indent = 32;
constexpr int option_padding = 12;

/**
 * @brief Make a muted label that wraps, for paths and notes.
 * @param text Its text.
 * @param parent Its parent.
 * @return The label.
 */
QLabel* muted(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    Theme::set_role(label, Theme::muted_label);
    label->setWordWrap(true);
    return label;
}

/**
 * @brief Make a label for a path, which can be selected and copied.
 * @param text Its text.
 * @param parent Its parent.
 * @return The label.
 */
QLabel* path_label(const QString& text, QWidget* parent) {
    QLabel* label = muted(text, parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

/**
 * @brief Make a section's heading, in bold.
 * @param text Its text.
 * @param parent Its parent.
 * @return The label.
 */
QLabel* heading(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

/**
 * @brief Make a thin line between two sections or two options.
 * @param parent Its parent.
 * @return The line.
 */
QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    Theme::set_role(line, Theme::separator);
    return line;
}

/**
 * @brief Put buttons side by side, left aligned.
 * @param buttons The buttons.
 * @param indent How far in from the left they start.
 * @return Their row.
 */
QHBoxLayout* button_row(std::initializer_list<QPushButton*> buttons, int indent = 0) {
    auto* row = new QHBoxLayout();
    row->setContentsMargins(indent, 0, 0, 0);

    for (QPushButton* button : buttons) {
        row->addWidget(button);
    }

    row->addStretch();
    return row;
}

}

QString GameFilesDialog::radio_name(const QString& id) {
    return QStringLiteral("editionRadio_") + id;
}

QString GameFilesDialog::set_up_name(const QString& id) {
    return QStringLiteral("setUpButton_") + id;
}

QString GameFilesDialog::status_name(const QString& id) {
    return QStringLiteral("editionStatus_") + id;
}

GameFilesDialog::GameFilesDialog(const std::vector<GameFilesEdition>& editions, const QString& active, const QString& games_root, bool playing, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(ui_text::game_files);
    setMinimumWidth(dialog_width);
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(6);

    layout->addWidget(heading(ui_text::game_discs_label, this));
    layout->addWidget(muted(ui_text::game_discs_note, this));
    auto* box = new QFrame(this);
    Theme::set_role(box, Theme::options_box);
    auto* options = new QVBoxLayout(box);
    options->setContentsMargins(option_padding, option_padding, option_padding, option_padding);
    options->setSpacing(6);
    radios_ = new QButtonGroup(this);
    bool first = true;

    for (const GameFilesEdition& edition : editions) {
        if (!first) {
            options->addSpacing(4);
            options->addWidget(separator(box));
            options->addSpacing(4);
        }

        first = false;
        auto* radio = new QRadioButton(QString(ui_text::play_edition).arg(edition.name), box);
        radio->setObjectName(radio_name(edition.id));
        radio->setChecked(edition.id == active);
        radio->setEnabled(!edition.files.isEmpty() && !playing);
        radio->setProperty("edition", edition.id);
        radios_->addButton(radio);
        options->addWidget(radio);

        QLabel* status = path_label(edition.files.isEmpty() ? QString(ui_text::not_set_up) : QString(ui_text::files_at).arg(edition.files), box);
        status->setObjectName(status_name(edition.id));
        status->setContentsMargins(option_indent, 0, 0, 0);
        options->addWidget(status);

        auto* set_up_button = new QPushButton(edition.files.isEmpty() ? ui_text::set_up : ui_text::set_up_again, box);
        set_up_button->setObjectName(set_up_name(edition.id));
        set_up_button->setEnabled(!playing);
        auto* show_files = new QPushButton(ui_text::show_files, box);
        show_files->setEnabled(!edition.files.isEmpty());
        options->addLayout(button_row({set_up_button, show_files}, option_indent));

        const QString id = edition.id;
        connect(set_up_button, &QPushButton::clicked, this, [this, id]() {
            emit set_up(id);
        });
        connect(show_files, &QPushButton::clicked, this, [this, id]() {
            emit show_game_files(id);
        });
    }

    layout->addWidget(box);

    layout->addSpacing(section_spacing);
    layout->addWidget(separator(this));
    layout->addSpacing(section_spacing);
    layout->addWidget(heading(ui_text::games_folder_label, this));
    layout->addWidget(muted(ui_text::games_root_note, this));
    games_root_ = path_label(QString(ui_text::folder_at).arg(games_root), this);
    layout->addWidget(games_root_);
    auto* change = new QPushButton(ui_text::change_folder, this);
    auto* verify = new QPushButton(ui_text::verify_selected, this);
    verify->setObjectName(verify_name);
    verify->setEnabled(!active.isEmpty());
    layout->addLayout(button_row({change, verify}));

    layout->addSpacing(section_spacing);
    layout->addWidget(separator(this));
    layout->addSpacing(section_spacing);
    layout->addWidget(heading(ui_text::remaster_optional_label, this));
    layout->addWidget(muted(ui_text::remaster_note, this));
    use_remaster_ = new QCheckBox(ui_text::use_remaster, this);
    use_remaster_->setObjectName(use_remaster_name);
    layout->addWidget(use_remaster_);
    remaster_where_ = path_label(QString(), this);
    remaster_where_->setObjectName(remaster_where_name);
    layout->addWidget(remaster_where_);
    auto* choose_remaster_button = new QPushButton(ui_text::choose_remaster, this);
    auto* find_remaster_button = new QPushButton(ui_text::find_remaster, this);
    layout->addLayout(button_row({choose_remaster_button, find_remaster_button}));

    layout->addSpacing(section_spacing);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(ui_text::close);
    layout->addWidget(buttons);

    active_ = active;
    connect(radios_, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        const QString id = button->property("edition").toString();

        if (id != active_) {
            active_ = id;
            emit version_chosen(id);
        }
    });
    connect(verify, &QPushButton::clicked, this, &GameFilesDialog::verify_game_files);
    connect(change, &QPushButton::clicked, this, &GameFilesDialog::change_games_root);
    connect(use_remaster_, &QCheckBox::toggled, this, &GameFilesDialog::use_remaster_changed);
    connect(choose_remaster_button, &QPushButton::clicked, this, &GameFilesDialog::choose_remaster);
    connect(find_remaster_button, &QPushButton::clicked, this, &GameFilesDialog::find_remaster);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void GameFilesDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    const int needed = layout()->totalHeightForWidth(width());

    if (needed > height()) {
        resize(width(), needed);
    }
}

void GameFilesDialog::set_remaster(bool use, const QString& where) {
    const QSignalBlocker blocker(use_remaster_);
    use_remaster_->setChecked(use);
    remaster_where_->setText(where);
}

void GameFilesDialog::set_games_root(const QString& path) {
    games_root_->setText(QString(ui_text::folder_at).arg(path));
}

}
