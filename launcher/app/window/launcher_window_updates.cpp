#include "window/launcher_window.h"

#include "pages/ready_page.h"
#include "pages/whats_new_panel.h"
#include "window/ui_text.h"

#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QStackedWidget>

#include <vector>

namespace lom::launcher {

void LauncherWindow::check_updates(bool manual) {
    manual_update_check_ = manual;
    updates_->check();
}

void LauncherWindow::show_update_result(const UpdateChecker::Result& result) {
    const bool manual = manual_update_check_;
    manual_update_check_ = false;

    switch (result.status) {
    case UpdateChecker::Result::Status::UpdateAvailable: {
        set_banner(QString(ui_text::update_banner).arg(result.latest_version));
        QMessageBox prompt(QMessageBox::Information, ui_text::update_available_title,
                           QString(ui_text::update_available_body).arg(result.latest_version, version_), QMessageBox::NoButton, this);
        QPushButton* update = prompt.addButton(ui_text::update_now, QMessageBox::AcceptRole);
        prompt.addButton(ui_text::not_now, QMessageBox::RejectRole);
        prompt.setDetailedText(result.release_notes);
        prompt.exec();

        if (prompt.clickedButton() == update && result.release_page.isValid()) {
            services_.desktop->open_url(result.release_page);
        }

        break;
    }

    case UpdateChecker::Result::Status::UpToDate:
        if (manual) {
            QMessageBox::information(this, ui_text::no_update_title, QString(ui_text::up_to_date).arg(version_));
        }

        break;

    case UpdateChecker::Result::Status::NotConfigured:
        if (manual) {
            QMessageBox::information(this, ui_text::no_update_title, ui_text::updates_not_configured);
        }

        break;

    case UpdateChecker::Result::Status::CouldNotCheck:
        if (manual) {
            QMessageBox::warning(this, ui_text::no_update_title, QString(ui_text::could_not_check).arg(result.error));
        }

        break;
    }
}

void LauncherWindow::show_whats_new() {
    whats_new_->open(QString(ui_text::whats_new_current).arg(version_));
    place_whats_new();

    if (!services_.updates) {
        whats_new_->show_message(ui_text::whats_new_not_configured);
        return;
    }

    services_.updates->fetch_releases([this](const UpdateFeed::Response& response) {
        QString error = response.error;
        const auto releases = response.ok ? ReleaseHistory::interpret(response.body, &error) : std::nullopt;

        if (releases) {
            whats_new_->show_releases(*releases, version_);
        } else {
            whats_new_->show_message(QString(ui_text::whats_new_failed).arg(error));
        }
    });
}

void LauncherWindow::show_whats_new_after_update() {
    if (!services_.updates || !settings_.check_updates_on_startup) {
        remember_this_version();
        return;
    }

    services_.updates->fetch_releases([this](const UpdateFeed::Response& response) {
        const auto releases = response.ok ? ReleaseHistory::interpret(response.body, nullptr) : std::nullopt;

        if (!releases) {
            return;
        }

        const QString last_seen = QString::fromStdString(settings_.last_seen_version);
        std::vector<ReleaseHistory::Release> since;

        for (const ReleaseHistory::Release& release : *releases) {
            if (ReleaseHistory::compare(release.version, last_seen) > 0 && ReleaseHistory::compare(release.version, version_) <= 0) {
                since.push_back(release);
            }
        }

        remember_this_version();

        if (!since.empty()) {
            whats_new_->open(QString(ui_text::whats_new_updated).arg(version_));
            place_whats_new();
            whats_new_->show_releases(since, version_);
        }
    });
}

void LauncherWindow::place_whats_new() {
    if (whats_new_ == nullptr || !whats_new_->isVisible()) {
        return;
    }

    constexpr int side = 40;
    constexpr int top = 26;
    constexpr int bottom = 22;
    constexpr int above_controls = 16;
    constexpr int widest = 960;

    uncover_page();
    QRect area = stack_->geometry().adjusted(side, top, -side, -bottom);

    if (stack_->currentWidget() == ready_) {
        const QWidget* controls = ready_->controls();
        const QRect buttons(controls->mapTo(centralWidget(), QPoint(0, 0)), controls->size());
        area.setLeft(buttons.left());
        area.setRight(buttons.right());
        area.setBottom(buttons.top() - above_controls);
        ready_->set_heading_visible(false);
    } else {
        if (area.width() > widest) {
            area.adjust((area.width() - widest) / 2, 0, -(area.width() - widest) / 2, 0);
        }

        covered_page_ = stack_->currentWidget();
        covered_page_->hide();
    }

    whats_new_->setGeometry(area);
    whats_new_->raise();
}

void LauncherWindow::uncover_page() {
    ready_->set_heading_visible(true);

    if (covered_page_ != nullptr && covered_page_ == stack_->currentWidget()) {
        covered_page_->show();
    }

    covered_page_ = nullptr;
}

void LauncherWindow::remember_this_version() {
    settings_.last_seen_version = version_.toStdString();
    save_settings_or_warn();
}

}
