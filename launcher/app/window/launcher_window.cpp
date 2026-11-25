#include "window/launcher_window.h"

#include "pages/notice_page.h"
#include "pages/problem_page.h"
#include "pages/progress_page.h"
#include "pages/ready_page.h"
#include "pages/welcome_page.h"
#include "pages/whats_new_panel.h"
#include "services/game_process.h"
#include "window/backdrop.h"
#include "window/background_task.h"
#include "window/theme.h"
#include "window/ui_text.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSize>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>

namespace lom::launcher {

namespace {
constexpr char ending_error[] = "it stopped with an error (code %1)";
constexpr char ending_graphics[] = "its graphics didn't start";
constexpr int window_width = 1100;
constexpr int window_height = 680;
constexpr int minimum_width = 860;
constexpr int minimum_height = 600;
constexpr int header_icon_size = 18;
constexpr int brand_icon_size = 26;
constexpr int header_margin = 10;
constexpr int bar_side_margin = 16;
}

LauncherWindow::LauncherWindow(AppPaths paths, LauncherServices services, QString version, QWidget* parent)
    : QMainWindow(parent), paths_(std::move(paths)), services_(std::move(services)), version_(std::move(version)) {
    ui_text::set_language(QString::fromStdString(LauncherSettings::read_language(paths_.settings_file())));
    setWindowTitle(ui_text::window_title);
    setWindowIcon(Theme::window_icon());
    setMinimumSize(minimum_width, minimum_height);
    resize(window_width, window_height);

    backdrop_ = new Backdrop(this);
    QWidget* central = backdrop_;
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* top_bar = new QWidget(central);
    Theme::set_role(top_bar, Theme::top_bar);
    top_bar->setAttribute(Qt::WA_StyledBackground);
    auto* header = new QHBoxLayout(top_bar);
    header->setContentsMargins(bar_side_margin, header_margin, bar_side_margin, header_margin);
    brand_icon_ = new QLabel(top_bar);
    brand_icon_->setPixmap(Theme::app_icon(brand_icon_size));
    header->addWidget(brand_icon_);
    auto* brand = new QLabel(ui_text::window_title, top_bar);
    Theme::set_role(brand, Theme::brand_label);
    header->addWidget(brand);
    header->addStretch();
    theme_toggle_ = new QToolButton(top_bar);
    theme_toggle_->setObjectName("themeToggle");
    Theme::set_role(theme_toggle_, Theme::top_bar_button);
    theme_toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    theme_toggle_->setIconSize(QSize(header_icon_size, header_icon_size));
    header->addWidget(theme_toggle_);
    help_ = new QToolButton(top_bar);
    Theme::set_role(help_, Theme::top_bar_button);
    help_->setText(ui_text::help);
    help_->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(help_);
    menu->addAction(ui_text::show_notice, this, &LauncherWindow::show_notice_again);
    menu->addAction(ui_text::whats_new, this, &LauncherWindow::show_whats_new);
    menu->addAction(ui_text::check_for_updates, this, [this]() {
        check_updates(true);
    });
    menu->addAction(ui_text::open_logs, this, [this]() {
        std::error_code code;
        std::filesystem::create_directories(paths_.logs_directory(), code);
        services_.desktop->open_folder(AppPaths::to_text(paths_.logs_directory()));
    });
    menu->addAction(ui_text::report_a_bug, this, &LauncherWindow::report_latest_run);
    menu->addSeparator();
    menu->addAction(ui_text::about, this, &LauncherWindow::show_about);
    help_->setMenu(menu);
    header->addWidget(help_);
    layout->addWidget(top_bar);

    banner_ = new QLabel(central);
    banner_->setObjectName("banner");
    Theme::set_role(banner_, Theme::banner_label);
    banner_->setAlignment(Qt::AlignCenter);
    banner_->setWordWrap(true);
    banner_->setVisible(false);
    auto* banner_row = new QHBoxLayout();
    banner_row->setContentsMargins(bar_side_margin, header_margin, bar_side_margin, 0);
    banner_row->addWidget(banner_);
    layout->addLayout(banner_row);

    stack_ = new QStackedWidget(central);
    notice_ = new NoticePage(stack_);
    welcome_ = new WelcomePage(stack_);
    progress_ = new ProgressPage(stack_);
    ready_ = new ReadyPage(stack_);
    problem_ = new ProblemPage(stack_);

    for (QWidget* page : std::initializer_list<QWidget*>{notice_, welcome_, progress_, ready_, problem_}) {
        stack_->addWidget(page);
    }

    layout->addWidget(stack_, 1);

    auto* status_bar = new QWidget(central);
    Theme::set_role(status_bar, Theme::status_bar);
    status_bar->setAttribute(Qt::WA_StyledBackground);
    auto* status_layout = new QHBoxLayout(status_bar);
    status_layout->setContentsMargins(bar_side_margin, header_margin / 2 + 2, bar_side_margin, header_margin / 2 + 2);
    auto* version_label = new QLabel(QStringLiteral("lom-native %1").arg(version_), status_bar);
    Theme::set_role(version_label, Theme::status_label);
    status_layout->addWidget(version_label);
    status_layout->addStretch();
    layout->addWidget(status_bar);
    setCentralWidget(central);
    whats_new_ = new WhatsNewPanel(central);

    task_ = new BackgroundTask(this);
    game_ = new GameProcess(*services_.processes, this);
    updates_ = new UpdateChecker(services_.updates.get(), version_, this);

    connect(theme_toggle_, &QToolButton::clicked, this, &LauncherWindow::toggle_theme);
    connect(notice_, &NoticePage::accepted, this, &LauncherWindow::accept_notice);
    connect(notice_, &NoticePage::quit_requested, this, &LauncherWindow::close);
    connect(welcome_, &WelcomePage::choose_image, this, &LauncherWindow::choose_image);
    connect(welcome_, &WelcomePage::choose_folder, this, &LauncherWindow::choose_folder);
    connect(welcome_, &WelcomePage::change_destination, this, [this]() {
        change_games_root(this);
    });
    connect(welcome_, &WelcomePage::back, this, &LauncherWindow::show_home);
    connect(progress_, &ProgressPage::cancel_requested, task_, &BackgroundTask::cancel);
    connect(whats_new_, &WhatsNewPanel::link_opened, this, [this](const QUrl& url) {
        services_.desktop->open_url(url);
    });
    connect(whats_new_, &WhatsNewPanel::closed, this, &LauncherWindow::uncover_page);
    connect(stack_, &QStackedWidget::currentChanged, this, &LauncherWindow::place_whats_new);
    stack_->installEventFilter(this);
    ready_->controls()->installEventFilter(this);
    connect(ready_, &ReadyPage::buttons_measured, this, [this]() {
        setMinimumWidth(std::max(minimum_width, ready_->minimum_width()));
    });
    connect(ready_, &ReadyPage::play, this, &LauncherWindow::launch);
    connect(ready_, &ReadyPage::settings, this, &LauncherWindow::open_settings);
    connect(ready_, &ReadyPage::mods, this, &LauncherWindow::open_mods);
    connect(ready_, &ReadyPage::game_files, this, &LauncherWindow::open_game_files);
    connect(ready_, &ReadyPage::browse_saves, this, &LauncherWindow::browse_saves);
    connect(ready_, &ReadyPage::open_saves, this, &LauncherWindow::open_saves);
    connect(ready_, &ReadyPage::import_card, this, &LauncherWindow::import_card);
    connect(problem_, &ProblemPage::primary, this, [this]() {
        if (problem_primary_) {
            problem_primary_();
        }
    });
    connect(problem_, &ProblemPage::secondary, this, [this]() {
        if (problem_secondary_) {
            problem_secondary_();
        }
    });
    connect(task_, &BackgroundTask::progressed, this, [this](int stage, quint64 done, quint64 total) {
        switch (Progress::Stage(stage)) {
        case Progress::Stage::CheckingImage:
            progress_->set_stage(ui_text::checking_image);
            break;

        case Progress::Stage::Extracting:
            progress_->set_stage(ui_text::extracting);
            break;

        case Progress::Stage::VerifyingFiles:
            progress_->set_stage(ui_text::verifying);
            break;

        case Progress::Stage::CheckingFolder:
        case Progress::Stage::ImportingSaves:
            break;
        }

        progress_->set_progress(done, total);
    });
    connect(task_, &BackgroundTask::finished, this, [this]() {
        auto done = std::move(task_done_);
        task_done_ = nullptr;

        if (done) {
            done();
        }
    });
    connect(game_, &GameProcess::started, this, [this]() {
        ready_->set_playing(true);
        hide();
    });
    connect(game_, &GameProcess::failed, this, [this](const QString& message) {
        ready_->set_playing(false);
        showNormal();
        raise();
        show_problem(
            {ui_text::launch_failed_title, QString(ui_text::launch_failed_body).arg(message, AppPaths::to_text(game_->log_file())), QString()}, ui_text::back,
            [this]() {
                show_home();
            },
            QString(), nullptr);
    });
    connect(game_, &GameProcess::exited, this, [this](int exit_code, GameProcess::Ending ending) {
        ready_->set_playing(false);
        showNormal();
        raise();
        activateWindow();
        refresh_ready();

        if (ending == GameProcess::Ending::Quit) {
            show_page(Page::Ready);
            return;
        }

        const QString log_file = AppPaths::to_text(game_->log_file());
        const bool graphics = ending == GameProcess::Ending::GraphicsUnavailable;
        const ProblemText::Message message =
            graphics ? ProblemText::Message{ui_text::graphics_unavailable_title, QString(ui_text::graphics_unavailable_body).arg(log_file), QString()}
                     : ProblemText::Message{ui_text::game_crashed_title, QString(ui_text::game_crashed_body).arg(exit_code).arg(log_file), QString()};
        std::filesystem::path run_prefix = game_->log_file();
        run_prefix.replace_extension();
        const QString how = graphics ? QString::fromLatin1(ending_graphics) : QString::fromLatin1(ending_error).arg(exit_code);
        show_problem(
            message, ui_text::report_this_bug,
            [this, run_prefix, how]() {
                report_bug(run_prefix, how);
            },
            ui_text::back,
            [this]() {
                show_home();
            });
    });
    connect(updates_, &UpdateChecker::checked, this, &LauncherWindow::show_update_result);
}

LauncherWindow::~LauncherWindow() {
    task_->cancel();
    task_->wait();
}

void LauncherWindow::start() {
    std::string error;

    if (settings_.load(paths_.settings_file(), &error) == LauncherSettings::LoadStatus::Corrupt) {
        set_banner(QStringLiteral("%1: %2").arg(QString(ui_text::settings_corrupt), QString::fromStdString(error)));
    }

    apply_theme();
    std::string catalog_error;

    if (!catalog_.load(paths_.catalog_file(), &catalog_error)) {
        show_problem(
            {ui_text::malformed_title, ui_text::data_files_damaged, QString::fromStdString(catalog_error)}, ui_text::notice_quit,
            [this]() {
                close();
            },
            QString(), nullptr);
        return;
    }

    if (ReleaseHistory::is_release(version_)) {
        updated_ = settings_.ownership_notice_acknowledged && ReleaseHistory::compare(version_, QString::fromStdString(settings_.last_seen_version)) > 0;

        if (!settings_.ownership_notice_acknowledged) {
            settings_.last_seen_version = version_.toStdString();
        }
    }

    if (!settings_.ownership_notice_acknowledged) {
        show_page(Page::Notice);
        return;
    }

    after_notice();
}

LauncherWindow::Page LauncherWindow::page() const {
    QWidget* current = stack_->currentWidget();

    if (current == notice_) {
        return Page::Notice;
    }

    if (current == welcome_) {
        return Page::Welcome;
    }

    if (current == progress_) {
        return Page::Progress;
    }

    return current == ready_ ? Page::Ready : Page::Problem;
}

const LauncherSettings& LauncherWindow::settings() const {
    return settings_;
}

QString LauncherWindow::banner() const {
    return banner_->text();
}

void LauncherWindow::wait_for_work() {
    while (task_->running()) {
        task_->wait();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents);
}

bool LauncherWindow::game_running() const {
    return game_->running();
}

void LauncherWindow::closeEvent(QCloseEvent* event) {
    if (game_->running()) {
        hide();
        event->ignore();
        return;
    }

    task_done_ = nullptr;
    task_->cancel();
    task_->wait();
    event->accept();
}

void LauncherWindow::show_page(Page page) {
    switch (page) {
    case Page::Notice:
        stack_->setCurrentWidget(notice_);
        notice_->focus_default();
        break;

    case Page::Welcome:
        stack_->setCurrentWidget(welcome_);
        welcome_->focus_default();
        break;

    case Page::Progress:
        stack_->setCurrentWidget(progress_);
        break;

    case Page::Ready:
        stack_->setCurrentWidget(ready_);
        ready_->focus_default();
        break;

    case Page::Problem:
        stack_->setCurrentWidget(problem_);
        break;
    }
}

void LauncherWindow::show_problem(const ProblemText::Message& message, const QString& primary_label, std::function<void()> primary,
                                  const QString& secondary_label, std::function<void()> secondary) {
    problem_primary_ = std::move(primary);
    problem_secondary_ = std::move(secondary);
    problem_->show_problem(message.title, message.body, message.details, primary_label, secondary_label);
    show_page(Page::Problem);
}

bool LauncherWindow::save_settings(QString* error) {
    std::string reason;

    if (settings_.save(paths_.settings_file(), &reason)) {
        return true;
    }

    *error = QString::fromStdString(reason);
    return false;
}

bool LauncherWindow::save_settings_or_warn() {
    QString error;

    if (save_settings(&error)) {
        return true;
    }

    set_banner(QStringLiteral("%1: %2").arg(QString(ui_text::settings_save_failed), error));
    return false;
}

void LauncherWindow::set_banner(const QString& text) {
    banner_->setText(text);
    banner_->setVisible(!text.isEmpty());
}

void LauncherWindow::apply_theme() {
    const bool dark = Theme::resolve_dark(settings_.theme);

    if (auto* application = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        Theme::apply(*application, dark);
    }

    theme_toggle_->setText(dark ? ui_text::toggle_to_light : ui_text::toggle_to_dark);
    theme_toggle_->setIcon(Theme::pixmap(dark ? Theme::Icon::Sun : Theme::Icon::Moon, header_icon_size));
    theme_toggle_->setAccessibleName(theme_toggle_->text());
    notice_->refresh_icons();
    welcome_->refresh_icons();
    progress_->refresh_icons();
    ready_->refresh_icons();
    problem_->refresh_icons();
    setWindowIcon(Theme::window_icon());
    backdrop_->update();
}

void LauncherWindow::toggle_theme() {
    settings_.theme = Theme::is_dark() ? LauncherSettings::Theme::Light : LauncherSettings::Theme::Dark;
    apply_theme();
    save_settings_or_warn();
}

void LauncherWindow::accept_notice() {
    settings_.ownership_notice_acknowledged = true;
    QString error;

    if (!save_settings(&error)) {
        settings_.ownership_notice_acknowledged = false;
        show_problem(
            {ui_text::notice_save_failed, QString(ui_text::notice_save_failed_body).arg(error), QString()}, ui_text::retry,
            [this]() {
                accept_notice();
            },
            ui_text::notice_quit,
            [this]() {
                close();
            });
        return;
    }

    after_notice();
}

void LauncherWindow::after_notice() {
    if (updated_) {
        updated_ = false;
        show_whats_new_after_update();
    }

    if (settings_.check_updates_on_startup && !startup_update_checked_) {
        startup_update_checked_ = true;
        check_updates(false);
    }

    DiscImporter::remove_stale_staging(games_root());

    if (settings_.active_installation) {
        validate_installation(false);
    } else {
        show_welcome();
    }
}

void LauncherWindow::show_home() {
    expected_release_.clear();
    if (settings_.active_installation && active_release() != nullptr) {
        refresh_ready();
        show_page(Page::Ready);
    } else {
        show_welcome();
    }
}

void LauncherWindow::show_welcome() {
    const ReleaseCatalog::Release* expected = expected_release_.empty() ? nullptr : catalog_.by_id(expected_release_);
    welcome_->set_body(expected ? QString(ui_text::set_up_edition_body).arg(QString::fromStdString(expected->display_name)) : QString(ui_text::welcome_body));
    welcome_->set_destination(AppPaths::to_text(games_root()));
    welcome_->set_back_visible(settings_.active_installation.has_value());
    show_page(Page::Welcome);
}

bool LauncherWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
        place_whats_new();
    }

    return QMainWindow::eventFilter(watched, event);
}

void LauncherWindow::show_notice_again() {
    QMessageBox box(QMessageBox::NoIcon, ui_text::notice_title, ui_text::notice_body, QMessageBox::Ok, this);
    box.setTextFormat(Qt::RichText);
    box.exec();
}

void LauncherWindow::show_about() {
    QMessageBox::about(this, ui_text::about, QString(ui_text::about_body).arg(version_.toHtmlEscaped()));
}

}
