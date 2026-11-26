#include "launcher_spec.h"

#include "../../support/test_harness.h"

#include <gtest/gtest.h>
#include "window/app_paths.h"
#include "window/launcher_services.h"
#include "window/launcher_window.h"
#include "lom/launcher/disc/disc_importer.h"
#include "pages/page_builder.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <cstdio>
#include <deque>
#include <fstream>

namespace lom::launcher::test {

using ::testing::_;
using ::testing::NiceMock;

namespace {

constexpr int poll_interval_ms = 5;
constexpr int dialog_poll_interval_ms = 10;
#ifdef _WIN32
constexpr char runtime_file[] = "lom.exe";
#else
constexpr char runtime_file[] = "lom";
#endif
constexpr char default_settings[] = R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light"})";
constexpr char default_release[] = R"({"tag_name": "v0.4.0", "html_url": "https://github.com/example/lom/releases/tag/v0.4.0", "body": "Notes for 0.4.0"})";

/**
 * @brief Escape text for a JSON string.
 * @param text Text.
 * @return Escaped text without quotes.
 */
std::string json_escaped(const std::string& text) {
    std::string out;

    for (const char c : text) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
        }

        out.push_back(c);
    }

    return out;
}

/**
 * @brief Replace every occurrence of a placeholder.
 * @param text Text to edit.
 * @param placeholder Placeholder.
 * @param value Replacement.
 */
void replace_all(std::string& text, const std::string& placeholder, const std::string& value) {
    for (std::size_t at = text.find(placeholder); at != std::string::npos; at = text.find(placeholder, at + value.size())) {
        text.replace(at, placeholder.size(), value);
    }
}

}

LauncherSpec::LauncherSpec()
    : edition(DiscFixture::create(TestHarness::instance().scratch_directory() / "edition")), profile(edition.directory.parent_path() / "profile"),
      owned_file_dialogs_(std::make_unique<NiceMock<MockFileDialogs>>()), owned_process_runner_(std::make_unique<NiceMock<MockProcessRunner>>()),
      owned_update_feed_(std::make_unique<NiceMock<MockUpdateFeed>>()), owned_desktop_(std::make_unique<NiceMock<MockDesktop>>()),
      owned_installer_(std::make_unique<NiceMock<MockUpdateInstaller>>()) {
    config_directory = profile / "config";
    data_directory = profile / "data";
    program_directory = profile / "program";
    std::filesystem::create_directories(program_directory);
    std::ofstream(program_directory / runtime_file) << "stand-in game program";
    qunsetenv("LOM_GAME_EXECUTABLE");
    qunsetenv("LOM_LAUNCHER_DATA");
    steam_directory = profile / "steam";
    std::filesystem::create_directories(steam_directory);
    qputenv("LOM_STEAM_ROOT", AppPaths::to_text(steam_directory).toUtf8());

    disc_image_choice = AppPaths::to_text(edition.cue);
    update_response.ok = true;
    update_response.body = default_release;
    releases_response.ok = true;
    releases_response.body = "[]";
    game_start_failure = QStringLiteral("No such file or directory");
    given_settings(default_settings);

    file_dialogs_mock = owned_file_dialogs_.get();
    ON_CALL(*file_dialogs_mock, choose_disc_image(_, _)).WillByDefault([this]() {
        return disc_image_choice;
    });
    ON_CALL(*file_dialogs_mock, choose_game_folder(_)).WillByDefault([this]() {
        return game_folder_choice;
    });
    ON_CALL(*file_dialogs_mock, choose_games_root(_, _)).WillByDefault([this]() {
        return games_root_choice;
    });
    ON_CALL(*file_dialogs_mock, choose_save_folder(_, _)).WillByDefault([this]() {
        return save_folder_choice;
    });
    ON_CALL(*file_dialogs_mock, choose_remaster_folder(_, _)).WillByDefault([this]() {
        return remaster_folder_choice;
    });
    ON_CALL(*file_dialogs_mock, choose_mod_folder(_)).WillByDefault([this]() {
        return mod_folder_choice;
    });
    ON_CALL(*file_dialogs_mock, choose_memory_card(_)).WillByDefault([this]() {
        return memory_card_choice;
    });

    process_runner_mock = owned_process_runner_.get();
    ON_CALL(*process_runner_mock, start(_)).WillByDefault([this](const ProcessRunner::Request& request) {
        game_launches.push_back(request);
        game_running_ = !game_fails_to_start;
        QTimer::singleShot(0, process_runner_mock, [this]() {
            if (game_fails_to_start) {
                emit process_runner_mock->failed(game_start_failure);
            } else {
                emit process_runner_mock->started();
            }
        });
    });
    ON_CALL(*process_runner_mock, running()).WillByDefault([this]() {
        return game_running_;
    });

    update_feed_mock = owned_update_feed_.get();
    ON_CALL(*update_feed_mock, fetch_latest_release(_)).WillByDefault([this](std::function<void(const UpdateFeed::Response&)> done) {
        update_requests++;
        QTimer::singleShot(0, [this, done = std::move(done)]() {
            done(update_response);
        });
    });
    ON_CALL(*update_feed_mock, fetch_releases(_)).WillByDefault([this](std::function<void(const UpdateFeed::Response&)> done) {
        release_list_requests++;
        QTimer::singleShot(0, [this, done = std::move(done)]() {
            done(releases_response);
        });
    });

    desktop_mock = owned_desktop_.get();
    ON_CALL(*desktop_mock, open_folder(_)).WillByDefault([this](const QString& folder) {
        opened_folders.push_back(folder);
    });
    ON_CALL(*desktop_mock, open_url(_)).WillByDefault([this](const QUrl& url) {
        opened_pages.push_back(url);
    });

    installer_mock = owned_installer_.get();
    ON_CALL(*installer_mock, package_name(_)).WillByDefault([this](const QString& version) {
        return can_update_itself ? QStringLiteral("lom-native-%1-linux-x86_64.AppImage").arg(version) : QString();
    });
    ON_CALL(*installer_mock, install(_, _, _))
        .WillByDefault([this](const UpdateInstaller::Request& request, std::function<void(const UpdateInstaller::Progress&)> progress,
                              std::function<void(const UpdateInstaller::Outcome&)> done) {
            installs.push_back(request);
            progress({1, 2, false});

            if (install_finishes) {
                progress({0, 0, true});
                done(install_outcome);
            } else {
                unfinished_install = std::move(done);
            }
        });
    ON_CALL(*installer_mock, cancel()).WillByDefault([this]() {
        install_cancels++;

        if (unfinished_install) {
            auto done = std::move(unfinished_install);
            unfinished_install = nullptr;
            done({false, true, QString()});
        }
    });
    ON_CALL(*installer_mock, restart()).WillByDefault([this]() {
        restarts++;
        return restart_starts;
    });
    ON_CALL(*installer_mock, clean_up()).WillByDefault([this]() {
        clean_ups++;
    });

    ON_CALL(clock_mock, now_unix_seconds()).WillByDefault([this]() {
        return now;
    });
    ON_CALL(random_mock, next()).WillByDefault([this]() {
        return ++random_counter_;
    });
}

LauncherSpec::~LauncherSpec() {
    if (window_) {
        if (game_running_) {
            the_game_exits(0);
        }

        window_->close();
        window_.reset();
    }
}

void LauncherSpec::given_settings(const std::string& json) {
    std::filesystem::create_directories(config_directory);
    std::ofstream(config_directory / "launcher.json", std::ios::binary | std::ios::trunc) << expand(json);
}

std::filesystem::path LauncherSpec::given_an_extracted_folder(const std::string& name) {
    const std::filesystem::path scratch = profile / ("extraction-" + name);
    const DiscImporter importer(edition.catalog);
    const auto installed = importer.install(importer.identify(edition.cue, Progress()), edition.manifest, scratch, Progress());
    const std::filesystem::path folder = profile / name;
    std::filesystem::copy(installed.installation.data_path, folder, std::filesystem::copy_options::recursive);
    std::filesystem::remove_all(scratch);
    return folder;
}

void LauncherSpec::given_the_game_is_set_up_from(const std::filesystem::path& folder, const std::string& more_settings) {
    given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light",
        "active_installation": {"kind": "external", "data_path": ")" +
                   json_escaped(folder.u8string()) + R"(", "release": "us", "manifest_sha256": "{manifest}", "source_sha1": "", "installation_id": ""})" +
                   (more_settings.empty() ? std::string() : ", " + more_settings) + "}");
}

void LauncherSpec::given_the_next_dialog_is_answered(std::function<void(QWidget* dialog)> answer) {
    dialog_answers_.push_back(std::move(answer));
}

void LauncherSpec::given_the_next_message_is_answered_with(const QString& button) {
    given_the_next_dialog_is_answered([button](QWidget* dialog) {
        for (QAbstractButton* candidate : dialog->findChildren<QAbstractButton*>()) {
            if (candidate->text().remove(QLatin1Char('&')) == button) {
                candidate->click();
                return;
            }
        }

        ADD_FAILURE() << "The dialog has no button labelled \"" << button.toStdString() << "\"";
    });
}

LauncherWindow& LauncherSpec::open_launcher() {
    LauncherServices services;
    services.dialogs = std::move(owned_file_dialogs_);
    services.processes = std::move(owned_process_runner_);

    if (update_feed_configured) {
        services.updates = std::move(owned_update_feed_);
    }

    services.desktop = std::move(owned_desktop_);

    if (installer_configured) {
        services.installer = std::move(owned_installer_);
    }
    services.issue_repository = QStringLiteral("example/lom");
    services.clock = &clock_mock;
    services.random = &random_mock;

    const AppPaths paths(config_directory, data_directory, edition.catalog_path.parent_path(), program_directory);
    window_ = std::make_unique<LauncherWindow>(paths, std::move(services), QString::fromLatin1(version));

    auto* watcher = new QTimer(window_.get());
    watcher->setInterval(dialog_poll_interval_ms);
    QObject::connect(watcher, &QTimer::timeout, [this]() {
        QWidget* dialog = QApplication::activeModalWidget();

        if (dialog == nullptr) {
            return;
        }

        if (auto* box = qobject_cast<QMessageBox*>(dialog)) {
            messages.push_back(box->text());
        }

        if (!dialog_answers_.empty()) {
            auto answer = std::move(dialog_answers_.front());
            dialog_answers_.pop_front();
            answer(dialog);
        } else if (auto* closable = qobject_cast<QDialog*>(dialog)) {
            closable->reject();
        }
    });
    watcher->start();

    window_->show();
    window_->start();
    settle();
    return *window_;
}

LauncherWindow& LauncherSpec::launcher() {
    return *window_;
}

void LauncherSpec::press(const char* object_name) {
    auto* button = window_->findChild<QPushButton*>(QString::fromLatin1(object_name));

    if (button == nullptr || !button->isVisible() || !button->isEnabled()) {
        ADD_FAILURE() << "Button \"" << object_name << "\" is missing, hidden or disabled";
        return;
    }

    button->click();
    settle();
}

void LauncherSpec::choose_help(const QString& entry) {
    for (QToolButton* button : window_->findChildren<QToolButton*>()) {
        if (button->menu() == nullptr) {
            continue;
        }

        for (QAction* action : button->menu()->actions()) {
            if (action->text() == entry) {
                action->trigger();
                settle();
                return;
            }
        }
    }

    ADD_FAILURE() << "The Help menu has no entry \"" << entry.toStdString() << "\"";
}

void LauncherSpec::settle() {
    for (int round = 0; round < 3; round++) {
        window_->wait_for_work();
        QApplication::processEvents(QEventLoop::AllEvents);
        QThread::msleep(poll_interval_ms);
        QApplication::processEvents(QEventLoop::AllEvents);
    }
}

void LauncherSpec::the_game_exits(int exit_code) {
    game_running_ = false;
    emit process_runner_mock->finished(exit_code, false);
    settle();
}

void LauncherSpec::the_game_crashes(int exit_code) {
    game_running_ = false;
    emit process_runner_mock->finished(exit_code, true);
    settle();
}

QString LauncherSpec::heading() const {
    auto* stack = window_->findChild<QStackedWidget*>();
    QWidget* page = stack == nullptr ? nullptr : stack->currentWidget();
    auto* label = page == nullptr ? nullptr : page->findChild<QLabel*>(QString::fromLatin1(PageBuilder::heading_name));
    return label == nullptr ? QString() : label->text();
}

QString LauncherSpec::text_of(const char* object_name) const {
    const QString name = QString::fromLatin1(object_name);

    if (auto* label = window_->findChild<QLabel*>(name)) {
        return label->text();
    }

    if (auto* text = window_->findChild<QPlainTextEdit*>(name)) {
        return text->toPlainText();
    }

    if (auto* text = window_->findChild<QTextEdit*>(name)) {
        return text->toPlainText();
    }

    return {};
}

Json LauncherSpec::read_json(const std::filesystem::path& path) const {
    std::ifstream in(path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto document = Json::parse(text, nullptr);
    return document ? *document : Json();
}

Json LauncherSpec::settings_member(const std::string& key) const {
    const Json settings = read_json(config_directory / "launcher.json");
    const Json* member = settings.find(key);
    return member == nullptr ? Json() : *member;
}

/**
 * @brief Compare a JSON document with an expected one written out in the specification.
 * @param actual Document read from disk.
 * @param expected Expected document text; "{...}" placeholders as in expand().
 * @return Success when equal; otherwise a failure showing both documents.
 */
::testing::AssertionResult LauncherSpec::json_is(const Json& actual, const std::string& expected) const {
    std::string error;
    const auto wanted = Json::parse(expand(expected), &error);

    if (!wanted) {
        return ::testing::AssertionFailure() << "the expected JSON is invalid: " << error;
    }

    std::string actual_text = actual.dump(false), wanted_text = wanted->dump(false);
#ifdef _WIN32
    replace_all(actual_text, "\\\\", "/");
    replace_all(wanted_text, "\\\\", "/");
#endif

    if (actual_text == wanted_text) {
        return ::testing::AssertionSuccess();
    }

    return ::testing::AssertionFailure() << "expected:\n" << wanted->dump(true) << "actual:\n" << actual.dump(true);
}

/**
 * @brief Substitute path placeholders: {profile}, {data}, {cards}, {logs}, {program}, {sha1}.
 * @param text Text with placeholders.
 * @return Text with JSON-safe paths.
 */
std::string LauncherSpec::expand(const std::string& text) const {
    std::string out = text;
    replace_all(out, "{profile}", json_escaped(profile.u8string()));
    replace_all(out, "{data}", json_escaped(data_directory.u8string()));
    replace_all(out, "{cards}", json_escaped((data_directory / "cards").u8string()));
    replace_all(out, "{logs}", json_escaped((data_directory / "logs").u8string()));
    replace_all(out, "{program}", json_escaped(program_directory.u8string()));
    replace_all(out, "{images}", json_escaped(edition.cue.parent_path().u8string()));
    replace_all(out, "{sha1}", edition.raw_sha1);
    replace_all(out, "{manifest}", edition.manifest.aggregate_sha256());
    return out;
}

QString LauncherSpec::shown(const std::filesystem::path& path) {
    return AppPaths::to_text(path);
}

QString LauncherSpec::native(const QString& path) {
    return QDir::toNativeSeparators(path);
}

const char* const LauncherSpec::game_program = runtime_file;

}
