#include "pages/whats_new_panel.h"

#include "window/theme.h"
#include "window/ui_text.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>

namespace lom::launcher {

namespace {

constexpr int panel_padding = 22;
constexpr int panel_spacing = 12;
constexpr int button_width = 120;
constexpr QChar middle_dot(0x00B7);

/**
 * @brief Push a release's own headings down two levels, under its version heading.
 * @param notes Markdown.
 * @return The notes with deeper headings.
 */
QString demote_headings(const QString& notes) {
    QStringList lines = notes.split(QLatin1Char('\n'));
    bool in_code = false;

    for (QString& line : lines) {
        if (line.startsWith(QLatin1String("```"))) {
            in_code = !in_code;
        } else if (!in_code && line.startsWith(QLatin1Char('#'))) {
            line.prepend(QLatin1String("##"));
        }
    }

    return lines.join(QLatin1Char('\n'));
}

/**
 * @brief The address of every release, from one release's page.
 * @param page Like https://github.com/owner/name/releases/tag/v2.3.1.
 * @return Like https://github.com/owner/name/releases, or empty.
 */
QUrl releases_page_from(const QUrl& page) {
    const QString path = page.path();
    const qsizetype tag = path.indexOf(QLatin1String("/releases/"));

    if (tag < 0) {
        return {};
    }

    QUrl all = page;
    all.setPath(path.left(tag + static_cast<qsizetype>(sizeof("/releases") - 1)));
    all.setQuery(QString());
    all.setFragment(QString());
    return all;
}

}

WhatsNewPanel::WhatsNewPanel(QWidget* parent) : QFrame(parent) {
    setObjectName(panel_name);
    Theme::set_role(this, Theme::notes_panel);
    setVisible(false);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(panel_padding, panel_padding, panel_padding, panel_padding);
    column->setSpacing(panel_spacing);

    auto* heading = new QLabel(QString(ui_text::whats_new), this);
    Theme::set_role(heading, Theme::hero_subtitle);
    column->addWidget(heading);
    summary_ = new QLabel(this);
    summary_->setObjectName(summary_name);
    summary_->setWordWrap(true);
    column->addWidget(summary_);

    notes_ = new QTextBrowser(this);
    notes_->setObjectName(notes_name);
    Theme::set_role(notes_, Theme::release_notes);
    notes_->setOpenLinks(false);
    notes_->setFrameShape(QFrame::NoFrame);
    QPalette palette = notes_->palette();
    palette.setColor(QPalette::Link, Theme::accent());
    notes_->setPalette(palette);
    column->addWidget(notes_, 1);

    auto* buttons = new QHBoxLayout();
    all_releases_ = new QPushButton(QString(ui_text::whats_new_all_releases), this);
    all_releases_->setObjectName(all_releases_name);
    Theme::set_role(all_releases_, Theme::notes_button);
    all_releases_->setVisible(false);
    buttons->addWidget(all_releases_);
    buttons->addStretch();
    close_ = new QPushButton(QString(ui_text::close), this);
    close_->setObjectName(close_name);
    Theme::set_role(close_, Theme::notes_button);
    close_->setMinimumWidth(button_width);
    close_->setDefault(true);
    buttons->addWidget(close_);
    column->addLayout(buttons);

    connect(notes_, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        if (url.scheme() == QLatin1String("https")) {
            emit link_opened(url);
        }
    });
    connect(all_releases_, &QPushButton::clicked, this, [this]() {
        emit link_opened(releases_page_);
    });
    connect(close_, &QPushButton::clicked, this, &WhatsNewPanel::close_panel);
}

void WhatsNewPanel::open(const QString& summary) {
    summary_->setText(summary);
    show_message(QString(ui_text::whats_new_loading));
    all_releases_->setVisible(false);
    show();
    raise();
    close_->setFocus();
}

void WhatsNewPanel::show_releases(const std::vector<ReleaseHistory::Release>& releases, const QString& current_version) {
    if (releases.empty()) {
        show_message(QString(ui_text::whats_new_none));
        return;
    }

    const bool english = ui_text::language() == QLatin1String("en");
    const QLocale locale = english ? QLocale(QLocale::English) : QLocale(ui_text::language());
    QString markdown;

    for (const ReleaseHistory::Release& release : releases) {
        QStringList details;

        if (release.published.isValid()) {
            details << (english ? locale.toString(release.published, QStringLiteral("MMMM d, yyyy")) : locale.toString(release.published, QLocale::LongFormat));
        }

        const int order = ReleaseHistory::compare(release.version, current_version);

        if (order == 0) {
            details << QString(ui_text::whats_new_this_version);
        } else if (order > 0) {
            details << QString(ui_text::whats_new_newer);
        }

        if (!markdown.isEmpty()) {
            markdown += QStringLiteral("\n\n---\n\n");
        }

        markdown += QStringLiteral("## %1\n\n").arg(release.version);

        if (!details.isEmpty()) {
            markdown += QStringLiteral("*%1*\n\n").arg(details.join(QStringLiteral(" %1 ").arg(middle_dot)));
        }

        markdown += demote_headings(release.notes.trimmed());
    }

    notes_->setMarkdown(markdown);

    QTextCharFormat heading;
    heading.setForeground(Theme::accent());

    for (QTextBlock block = notes_->document()->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0) {
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(heading);
        }
    }

    notes_->moveCursor(QTextCursor::Start);
    releases_page_ = releases_page_from(releases.front().page);
    all_releases_->setVisible(releases_page_.isValid() && !releases_page_.isEmpty());
}

void WhatsNewPanel::show_message(const QString& text) {
    notes_->setPlainText(text);
}

void WhatsNewPanel::close_panel() {
    hide();
    emit closed();
}

void WhatsNewPanel::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close_panel();
        return;
    }

    QWidget::keyPressEvent(event);
}

}
