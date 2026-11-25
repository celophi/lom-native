#include "pages/notice_page.h"

#include "pages/page_builder.h"
#include "window/ui_text.h"

#include <QLabel>
#include <QPushButton>

namespace lom::launcher {

namespace {

constexpr int heading_gap = 4;

}

NoticePage::NoticePage(QWidget* parent) : QWidget(parent) {
    PageBuilder builder(this);
    icon_ = builder.icon(Theme::Icon::Disc);
    builder.label(ui_text::notice_title, Theme::heading_label);
    builder.space(heading_gap);
    QLabel* body = builder.label(ui_text::notice_body, "");
    body->setObjectName(body_name);
    body->setTextFormat(Qt::RichText);
    body->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    builder.space(heading_gap * 3);
    accept_ = builder.button(ui_text::notice_accept, Theme::primary_button);
    accept_->setObjectName(accept_name);
    QPushButton* quit = builder.button(ui_text::notice_quit, Theme::secondary_button);
    quit->setObjectName(quit_name);
    builder.finish();
    connect(accept_, &QPushButton::clicked, this, &NoticePage::accepted);
    connect(quit, &QPushButton::clicked, this, &NoticePage::quit_requested);
}

void NoticePage::refresh_icons() {
    icon_->setPixmap(Theme::pixmap(Theme::Icon::Disc, PageBuilder::icon_size));
}

void NoticePage::focus_default() {
    accept_->setFocus();
}

}
