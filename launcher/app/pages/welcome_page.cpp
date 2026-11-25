#include "pages/welcome_page.h"

#include "pages/page_builder.h"
#include "window/ui_text.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace lom::launcher {

namespace {

constexpr int section_gap = 8;

}

WelcomePage::WelcomePage(QWidget* parent) : QWidget(parent) {
    PageBuilder builder(this);
    icon_ = builder.icon(Theme::Icon::Disc);
    builder.label(ui_text::window_title, Theme::title_label);
    builder.label(ui_text::welcome_heading, Theme::heading_label);
    body_ = builder.label(ui_text::welcome_body, Theme::muted_label);
    body_->setObjectName(body_name);
    builder.space(section_gap);

    QLabel* image_label = builder.label(ui_text::image_label, "");
    image_label->setAlignment(Qt::AlignLeft);
    auto* row = new QWidget(this);
    auto* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    image_path_ = new QLineEdit(row);
    image_path_->setReadOnly(true);
    image_path_->setPlaceholderText(ui_text::image_placeholder);
    image_path_->setAccessibleName(ui_text::image_label);
    image_label->setBuddy(image_path_);
    browse_ = new QPushButton(ui_text::browse, row);
    browse_->setObjectName(browse_name);
    Theme::set_role(browse_, Theme::primary_button);
    browse_->setCursor(Qt::PointingHandCursor);
    row_layout->addWidget(image_path_, 1);
    row_layout->addWidget(browse_);
    builder.add(row);

    builder.label(ui_text::destination_label, Theme::muted_label);
    destination_ = builder.label(QString(), Theme::muted_label);
    destination_->setObjectName(destination_label_name);
    QPushButton* destination = builder.button(ui_text::change_folder, Theme::link_button);
    destination->setObjectName(destination_name);
    builder.space(section_gap);

    QPushButton* folder = builder.button(ui_text::use_folder, Theme::link_button);
    folder->setObjectName(folder_name);

    back_ = builder.button(ui_text::back, Theme::secondary_button);
    back_->setObjectName(back_name);
    back_->setVisible(false);
    builder.finish();

    connect(browse_, &QPushButton::clicked, this, &WelcomePage::choose_image);
    connect(folder, &QPushButton::clicked, this, &WelcomePage::choose_folder);
    connect(destination, &QPushButton::clicked, this, &WelcomePage::change_destination);
    connect(back_, &QPushButton::clicked, this, &WelcomePage::back);
}

void WelcomePage::set_image_path(const QString& path) {
    image_path_->setText(path);
    image_path_->setToolTip(path);
    image_path_->setCursorPosition(0);
}

void WelcomePage::set_destination(const QString& path) {
    destination_->setText(path);
}

void WelcomePage::set_body(const QString& text) {
    body_->setText(text);
}

void WelcomePage::set_back_visible(bool visible) {
    back_->setVisible(visible);
}

void WelcomePage::refresh_icons() {
    icon_->setPixmap(Theme::pixmap(Theme::Icon::Disc, PageBuilder::icon_size));
}

void WelcomePage::focus_default() {
    browse_->setFocus();
}

}
