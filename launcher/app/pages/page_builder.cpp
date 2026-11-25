#include "pages/page_builder.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstring>

namespace lom::launcher {

namespace {

constexpr int page_margin = 24;
constexpr int card_padding = 30;
constexpr int column_spacing = 10;

}

PageBuilder::PageBuilder(QWidget* page) : page_(page) {
    auto* outer = new QHBoxLayout(page);
    outer->setContentsMargins(page_margin, page_margin, page_margin, page_margin);
    outer->addStretch();
    auto* card = new QFrame(page);
    Theme::set_role(card, Theme::card);
    card->setFixedWidth(content_width + 2 * card_padding);
    column_ = new QVBoxLayout(card);
    column_->setContentsMargins(card_padding, card_padding, card_padding, card_padding);
    column_->setSpacing(column_spacing);
    outer->addWidget(card, 0, Qt::AlignVCenter);
    outer->addStretch();
}

QLabel* PageBuilder::icon(Theme::Icon icon) {
    auto* label = new QLabel(page_);
    label->setPixmap(Theme::pixmap(icon, icon_size));
    label->setAlignment(Qt::AlignCenter);
    label->setAccessibleName(QString());
    column_->addWidget(label);
    return label;
}

QLabel* PageBuilder::label(const QString& text, const char* role) {
    auto* label = new QLabel(text, page_);

    if (role != nullptr && role[0] != '\0') {
        Theme::set_role(label, role);
    }

    if (role != nullptr && std::strcmp(role, Theme::heading_label) == 0) {
        label->setObjectName(heading_name);
    }

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    column_->addWidget(label);
    return label;
}

QPushButton* PageBuilder::button(const QString& text, const char* role) {
    auto* button = new QPushButton(text, page_);
    Theme::set_role(button, role);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoDefault(true);
    column_->addWidget(button);
    return button;
}

void PageBuilder::space(int pixels) {
    column_->addSpacing(pixels);
}

void PageBuilder::add(QWidget* widget) {
    column_->addWidget(widget);
}

void PageBuilder::finish() {
}

QVBoxLayout* PageBuilder::layout() {
    return column_;
}

}
