#include "pages/progress_page.h"

#include "pages/page_builder.h"
#include "window/ui_text.h"

#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>

namespace lom::launcher {

namespace {
constexpr int bar_maximum = 1000;
constexpr int section_gap = 8;

}

ProgressPage::ProgressPage(QWidget* parent) : QWidget(parent) {
    PageBuilder builder(this);
    icon_ = builder.icon(Theme::Icon::Disc);
    builder.label(ui_text::window_title, Theme::title_label);
    stage_ = builder.label(QString(), Theme::heading_label);
    subject_ = builder.label(QString(), Theme::muted_label);
    edition_ = builder.label(QString(), "");
    builder.space(section_gap);
    bar_ = new QProgressBar(this);
    bar_->setRange(0, bar_maximum);
    bar_->setTextVisible(false);
    builder.add(bar_);
    amount_ = builder.label(QString(), Theme::muted_label);
    builder.space(section_gap);
    cancel_ = builder.button(ui_text::cancel, Theme::secondary_button);
    cancel_->setObjectName(cancel_name);
    builder.finish();
    connect(cancel_, &QPushButton::clicked, this, &ProgressPage::cancel_requested);
}

void ProgressPage::begin(const QString& stage, const QString& subject) {
    stage_->setText(stage);
    subject_->setText(subject);
    edition_->clear();
    edition_->setVisible(false);
    bar_->setValue(0);
    bar_->setAccessibleName(stage);
    amount_->clear();
    cancel_->setEnabled(true);
    cancel_->setFocus();
}

void ProgressPage::set_stage(const QString& stage) {
    if (stage_->text() != stage) {
        stage_->setText(stage);
        bar_->setAccessibleName(stage);
        bar_->setValue(0);
    }
}

void ProgressPage::set_edition(const QString& edition) {
    edition_->setText(edition);
    edition_->setVisible(!edition.isEmpty());
}

void ProgressPage::set_progress(quint64 done, quint64 total) {
    const int value = total == 0 ? 0 : int((done * bar_maximum) / total);
    bar_->setValue(qBound(0, value, bar_maximum));
    const QLocale locale;
    amount_->setText(total == 0 ? QString()
                                : QString(ui_text::progress_bytes).arg(locale.formattedDataSize(qint64(done)), locale.formattedDataSize(qint64(total))));
}

QString ProgressPage::stage() const {
    return stage_->text();
}

void ProgressPage::refresh_icons() {
    icon_->setPixmap(Theme::pixmap(Theme::Icon::Disc, PageBuilder::icon_size));
}

}
