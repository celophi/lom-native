#include "pages/problem_page.h"

#include "pages/page_builder.h"
#include "window/ui_text.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>

namespace lom::launcher {

namespace {

constexpr int details_height = 130;
constexpr int section_gap = 8;

}

ProblemPage::ProblemPage(QWidget* parent) : QWidget(parent) {
    PageBuilder builder(this);
    icon_ = builder.icon(Theme::Icon::Warning);
    title_ = builder.label(QString(), Theme::heading_label);
    body_ = builder.label(QString(), "");
    body_->setObjectName(body_name);
    builder.space(section_gap);
    toggle_ = new QToolButton(this);
    toggle_->setText(ui_text::details);
    toggle_->setCheckable(true);
    toggle_->setArrowType(Qt::RightArrow);
    toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    builder.add(toggle_);
    details_ = new QPlainTextEdit(this);
    details_->setReadOnly(true);
    details_->setFixedHeight(details_height);
    details_->setVisible(false);
    details_->setAccessibleName(ui_text::details);
    details_->setObjectName(details_name);
    builder.add(details_);
    builder.space(section_gap);
    primary_ = builder.button(QString(), Theme::primary_button);
    primary_->setObjectName(primary_name);
    secondary_ = builder.button(QString(), Theme::secondary_button);
    secondary_->setObjectName(secondary_name);
    builder.finish();

    connect(toggle_, &QToolButton::toggled, this, [this](bool open) {
        details_->setVisible(open);
        toggle_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    });
    connect(primary_, &QPushButton::clicked, this, &ProblemPage::primary);
    connect(secondary_, &QPushButton::clicked, this, &ProblemPage::secondary);
}

void ProblemPage::show_problem(const QString& title, const QString& body, const QString& details, const QString& primary, const QString& secondary) {
    title_->setText(title);
    body_->setText(body);
    details_->setPlainText(details);
    toggle_->setChecked(false);
    toggle_->setVisible(!details.isEmpty());
    primary_->setText(primary);
    secondary_->setText(secondary);
    secondary_->setVisible(!secondary.isEmpty());
    primary_->setFocus();
}

void ProblemPage::refresh_icons() {
    icon_->setPixmap(Theme::pixmap(Theme::Icon::Warning, PageBuilder::icon_size));
}

}
