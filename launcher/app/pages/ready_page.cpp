#include "pages/ready_page.h"

#include "pages/page_builder.h"
#include "window/theme.h"
#include "window/ui_text.h"

#include <QFrame>
#include <QEvent>
#include <QHBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace lom::launcher {

namespace {

constexpr int side_margin = 40;
constexpr int top_margin = 26;
constexpr int bottom_margin = 22;
constexpr int panel_width = 960;
constexpr int panel_padding = 16;
constexpr int panel_spacing = 12;
constexpr int play_height = 64;
constexpr int save_path_minimum_width = 80;

}

ReadyPage::ReadyPage(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(side_margin, top_margin, side_margin, bottom_margin);
    outer->setSpacing(2);

    auto hero = [this](const QString& text, const char* role) {
        auto* label = new QLabel(text, this);
        Theme::set_role(label, role);
        Theme::add_text_shadow(label);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return label;
    };
    heading_ = hero(ui_text::ready_heading, Theme::hero_subtitle);
    heading_->setObjectName(PageBuilder::heading_name);
    outer->addWidget(heading_);
    edition_ = hero(QString(), Theme::hero_detail);
    edition_->setObjectName(edition_name);
    outer->addWidget(edition_);
    remaster_ = hero(QString(), Theme::hero_detail);
    remaster_->setObjectName(remaster_status_name);
    remaster_->setVisible(false);
    outer->addWidget(remaster_);
    outer->addStretch(1);

    auto* panel = new QFrame(this);
    controls_ = panel;
    Theme::set_role(panel, Theme::glass_panel);
    panel->setMaximumWidth(panel_width);
    auto* panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(panel_padding, panel_padding, panel_padding, panel_padding);
    panel_layout->setSpacing(panel_spacing);

    play_ = new QPushButton(ui_text::play, panel);
    play_->setObjectName(play_name);
    Theme::set_role(play_, Theme::play_button);
    play_->setMinimumHeight(play_height);
    play_->setCursor(Qt::PointingHandCursor);
    play_->setAutoDefault(true);
    panel_layout->addWidget(play_);

    auto* settings = new QPushButton(ui_text::settings, panel);
    settings->setObjectName(settings_name);
    auto* mods = new QPushButton(ui_text::mods, panel);
    mods->setObjectName(mods_name);
    auto* files_button = new QPushButton(ui_text::game_files, panel);
    files_button->setObjectName(game_files_name);
    browse_saves_ = new QPushButton(ui_text::browse, panel);
    browse_saves_->setObjectName(browse_saves_name);
    browse_saves_->setAccessibleName(QString(ui_text::save_folder_label) + " " + ui_text::browse);
    auto* open_saves = new QPushButton(ui_text::open_folder, panel);
    open_saves->setObjectName(open_saves_name);
    import_card_ = new QPushButton(ui_text::import_card, panel);
    import_card_->setObjectName(import_card_name);

    row_buttons_ = {settings, mods, files_button, browse_saves_, open_saves, import_card_};

    for (QPushButton* button : row_buttons_) {
        Theme::set_role(button, Theme::glass_button);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    auto* game_row = new QHBoxLayout();

    for (QPushButton* button : {settings, mods, files_button}) {
        game_row->addWidget(button, 1);
    }

    panel_layout->addLayout(game_row);

    auto* path_row = new QHBoxLayout();
    auto* save_label = new QLabel(QString(ui_text::save_folder_label) + QLatin1Char(':'), panel);
    save_folder_ = new QLabel(panel);
    save_folder_->setObjectName(save_folder_name);
    save_folder_->setToolTip(ui_text::save_folder_note);
    save_folder_->setMinimumWidth(save_path_minimum_width);
    save_folder_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    save_folder_->setWordWrap(true);
    save_folder_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    path_row->addWidget(save_label, 0, Qt::AlignTop);
    path_row->addWidget(save_folder_, 1);

    auto* saves_row = new QHBoxLayout();

    for (QPushButton* button : {browse_saves_, open_saves, import_card_}) {
        saves_row->addWidget(button, 1);
    }

    panel_layout->addLayout(saves_row);
    panel_layout->addLayout(path_row);

    status_ = new QLabel(panel);
    status_->setObjectName(status_name);
    status_->setAlignment(Qt::AlignHCenter);
    status_->setWordWrap(true);
    status_->setVisible(false);
    panel_layout->addWidget(status_);

    auto* panel_row = new QHBoxLayout();
    panel_row->addStretch();
    panel_row->addWidget(panel, 1);
    panel_row->addStretch();
    outer->addLayout(panel_row);

    connect(play_, &QPushButton::clicked, this, &ReadyPage::play);
    connect(settings, &QPushButton::clicked, this, &ReadyPage::settings);
    connect(mods, &QPushButton::clicked, this, &ReadyPage::mods);
    connect(files_button, &QPushButton::clicked, this, &ReadyPage::game_files);
    connect(browse_saves_, &QPushButton::clicked, this, &ReadyPage::browse_saves);
    connect(open_saves, &QPushButton::clicked, this, &ReadyPage::open_saves);
    connect(import_card_, &QPushButton::clicked, this, &ReadyPage::import_card);
}

void ReadyPage::set_edition(const QString& edition) {
    edition_->setText(QString(ui_text::installed_edition).arg(edition));
}

void ReadyPage::set_remaster_status(const QString& text) {
    remaster_->setText(text);
    remaster_->setVisible(!text.isEmpty() && edition_->isVisibleTo(this));
}

void ReadyPage::set_save_folder(const QString& path) {
    save_folder_->setText(path);
    save_folder_->setToolTip(QStringLiteral("%1\n%2").arg(path, QString(ui_text::save_folder_note)));
}

void ReadyPage::set_status(const QString& text) {
    status_->setText(text);
    status_->setVisible(!text.isEmpty());
}

void ReadyPage::set_playing(bool playing) {
    play_->setEnabled(!playing);
    play_->setText(playing ? ui_text::playing : ui_text::play);
    browse_saves_->setEnabled(!playing);
    import_card_->setEnabled(!playing);
}

QString ReadyPage::status() const {
    return status_->text();
}

QWidget* ReadyPage::controls() const {
    return controls_;
}

void ReadyPage::set_heading_visible(bool visible) {
    heading_->setVisible(visible);
    edition_->setVisible(visible);
    remaster_->setVisible(visible && !remaster_->text().isEmpty());
}

void ReadyPage::refresh_icons() {
}

void ReadyPage::focus_default() {
    play_->setFocus();
}

void ReadyPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    equalize_buttons();
}

void ReadyPage::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);

    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange) {
        QTimer::singleShot(0, this, &ReadyPage::equalize_buttons);
    }
}

void ReadyPage::equalize_buttons() {
    int widest = 0;

    for (QPushButton* button : row_buttons_) {
        button->setMinimumWidth(0);
        button->ensurePolished();
        widest = std::max(widest, button->sizeHint().width());
    }

    for (QPushButton* button : row_buttons_) {
        button->setMinimumWidth(widest);
    }

    controls_->layout()->invalidate();
    emit buttons_measured();
}

int ReadyPage::minimum_width() const {
    return controls_->minimumSizeHint().width() + 2 * side_margin;
}

}
