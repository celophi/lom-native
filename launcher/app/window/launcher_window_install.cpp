#include "window/launcher_window.h"
#include "lom/launcher/common/files_provider.h"
#include "services/launcher_dialogs.h"

#include "pages/progress_page.h"
#include "pages/ready_page.h"
#include "pages/welcome_page.h"
#include "window/background_task.h"
#include "window/ui_text.h"

#include <QDir>

#include <filesystem>
#include <istream>
#include <ostream>
#include <string>
#include <system_error>

namespace lom::launcher {

namespace {
constexpr char write_probe_name[] = ".lom-write-check";

/**
 * @brief Check that a folder can be created and written.
 * @param directory Folder.
 * @return True when writable.
 */
bool writable_directory(const std::filesystem::path& directory) {
    std::error_code code;
    files_provider().create_directories(directory, code);

    if (code) {
        return false;
    }

    const std::filesystem::path probe = directory / write_probe_name;
    {
        const auto out_file = files_provider().open_write(probe, std::ios::binary);
        std::ostream& out = *out_file;

        if (!out) {
            return false;
        }
    }

    files_provider().remove(probe, code);
    return true;
}
}

void LauncherWindow::validate_installation(bool full_check) {
    const Installation installation = *settings_.active_installation;
    const ReleaseCatalog::Release* release = catalog_.by_id(installation.release_id);
    const FileManifest* manifest = release == nullptr ? nullptr : manifest_for(*release);
    const QString folder = AppPaths::to_text(installation.data_path);

    if (manifest == nullptr || manifest->aggregate_sha256() != installation.manifest_sha256) {
        show_problem(
            {ui_text::folder_changed_title, QString(ui_text::folder_changed_body).arg(folder), QString()}, ui_text::choose_another,
            [this]() {
                show_welcome();
            },
            QString(), nullptr);
        return;
    }

    std::error_code code;
    const bool receipt_ok =
        installation.kind == Installation::Kind::External || files_provider().exists(installation.data_path.parent_path() / DiscImporter::receipt_file, code);
    const std::filesystem::path snapshot_file = paths_.snapshot_file(installation.data_path, installation.manifest_sha256);
    const std::optional<FileSnapshot> trusted = full_check ? std::nullopt : FileSnapshot::load(snapshot_file);
    auto result = std::make_shared<FolderVerifier::Result>();
    progress_->begin(full_check ? ui_text::checking_folder : ui_text::checking_installed, folder);
    show_page(Page::Progress);

    task_done_ = [this, result, folder, full_check, receipt_ok, snapshot_file]() {
        if (result->status == FolderVerifier::Result::Status::Cancelled) {
            show_problem(
                {ui_text::folder_changed_title, QString(ui_text::folder_changed_body).arg(folder), QString()}, ui_text::retry,
                [this, full_check]() {
                    validate_installation(full_check);
                },
                ui_text::choose_another,
                [this]() {
                    show_welcome();
                });
            return;
        }

        if (result->status == FolderVerifier::Result::Status::Verified && receipt_ok) {
            save_snapshot(result->snapshot, snapshot_file);
            show_home();
            return;
        }

        const ProblemText::Message message = ProblemText::for_folder(*result, folder, true);
        show_problem(
            message, ui_text::retry,
            [this, full_check]() {
                validate_installation(full_check);
            },
            ui_text::choose_another,
            [this]() {
                show_welcome();
            });
    };

    const FileManifest expected = *manifest;
    task_->start([result, installation, expected, trusted](const Progress& progress) {
        *result = FolderVerifier::check(installation.data_path, expected, trusted ? &*trusted : nullptr, progress);
    });
}

void LauncherWindow::save_snapshot(const FileSnapshot& snapshot, const std::filesystem::path& file) {
    std::string error;

    if (!snapshot.save(file, &error)) {
        qWarning("could not save the verification snapshot: %s", error.c_str());
    }
}

void LauncherWindow::choose_image() {
    if (task_->running()) {
        return;
    }

    QString start_directory = AppPaths::to_text(settings_.last_image_directory);

    if (start_directory.isEmpty()) {
        start_directory = QDir::homePath();
    }

    const QString chosen = LauncherDialogs(*services_.dialogs).choose_disc_image(this, start_directory);

    if (chosen.isEmpty()) {
        return;
    }

    welcome_->set_image_path(QDir::toNativeSeparators(chosen));
    identify_image(AppPaths::to_path(chosen));
}

void LauncherWindow::identify_image(const std::filesystem::path& path) {
    auto identification = std::make_shared<DiscImporter::Identification>();
    progress_->begin(ui_text::checking_image, AppPaths::to_text(path.filename()));
    show_page(Page::Progress);

    task_done_ = [this, identification, path]() {
        using Status = DiscImporter::Identification::Status;

        if (identification->status == Status::Cancelled) {
            show_welcome();
            return;
        }

        if (identification->status != Status::Supported) {
            show_problem(
                ProblemText::for_image(*identification, catalog_), ui_text::choose_another,
                [this]() {
                    show_welcome();
                },
                ui_text::back,
                [this]() {
                    show_home();
                });
            return;
        }

        const ReleaseCatalog::Release& release = *identification->release;

        if (release.runtime.empty()) {
            show_problem(
                {ui_text::runtime_title, QString(ui_text::runtime_body).arg(QString::fromStdString(release.display_name)), QString()}, ui_text::choose_another,
                [this]() {
                    show_welcome();
                },
                ui_text::back,
                [this]() {
                    show_home();
                });
            return;
        }

        if (wrong_edition(release)) {
            return;
        }

        settings_.last_image_directory = path.parent_path();
        install_image(*identification);
    };

    const ReleaseCatalog* catalog = &catalog_;
    const ClockProvider* clock = services_.clock;
    RandomProvider* random = services_.random;
    task_->start([identification, path, catalog, clock, random](const Progress& progress) {
        *identification = DiscImporter(*catalog, *clock, *random).identify(path, progress);
    });
}

void LauncherWindow::install_image(const DiscImporter::Identification& identification) {
    const ReleaseCatalog::Release& release = *identification.release;
    const FileManifest* manifest = manifest_for(release);

    if (manifest == nullptr) {
        show_problem(
            {ui_text::region_title, QString(ui_text::region_body).arg(QString::fromStdString(release.display_name)), QString()}, ui_text::choose_another,
            [this]() {
                show_welcome();
            },
            ui_text::back,
            [this]() {
                show_home();
            });
        return;
    }

    const std::filesystem::path root = games_root();
    auto result = std::make_shared<DiscImporter::InstallResult>();
    progress_->begin(ui_text::extracting, AppPaths::to_text(identification.selected.filename()));
    progress_->set_edition(QString(ui_text::recognized_edition).arg(QString::fromStdString(release.display_name)));
    show_page(Page::Progress);

    task_done_ = [this, result, root]() {
        if (result->status == DiscImporter::InstallResult::Status::Cancelled) {
            show_welcome();
            return;
        }

        if (result->status != DiscImporter::InstallResult::Status::Installed) {
            show_problem(
                ProblemText::for_install(*result, AppPaths::to_text(root)), ui_text::choose_another,
                [this]() {
                    show_welcome();
                },
                ui_text::change_folder,
                [this]() {
                    if (change_games_root(this)) {
                        show_welcome();
                    }
                });
            return;
        }

        if (result->snapshot) {
            save_snapshot(*result->snapshot, paths_.snapshot_file(result->installation.data_path, result->installation.manifest_sha256));
        }

        activate(result->installation);
    };

    const ReleaseCatalog* catalog = &catalog_;
    const ClockProvider* clock = services_.clock;
    RandomProvider* random = services_.random;
    const FileManifest expected = *manifest;
    const DiscImporter::Identification accepted = identification;
    task_->start([result, catalog, clock, random, expected, accepted, root](const Progress& progress) {
        *result = DiscImporter(*catalog, *clock, *random).install(accepted, expected, root, progress);
    });
}

void LauncherWindow::choose_folder() {
    if (task_->running()) {
        return;
    }

    const QString chosen = LauncherDialogs(*services_.dialogs).choose_game_folder(this);

    if (!chosen.isEmpty()) {
        verify_folder(AppPaths::to_path(chosen));
    }
}

void LauncherWindow::verify_folder(const std::filesystem::path& path) {
    auto result = std::make_shared<FolderVerifier::Result>();
    progress_->begin(ui_text::checking_folder, AppPaths::to_text(path));
    show_page(Page::Progress);

    task_done_ = [this, result, path]() {
        if (result->status == FolderVerifier::Result::Status::Cancelled) {
            show_welcome();
            return;
        }

        if (result->status != FolderVerifier::Result::Status::Verified) {
            show_problem(
                ProblemText::for_folder(*result, AppPaths::to_text(path), false), ui_text::choose_another,
                [this]() {
                    show_welcome();
                },
                ui_text::back,
                [this]() {
                    show_home();
                });
            return;
        }

        const ReleaseCatalog::Release& release = *result->release;

        if (release.runtime.empty()) {
            show_problem(
                {ui_text::runtime_title, QString(ui_text::runtime_body).arg(QString::fromStdString(release.display_name)), QString()}, ui_text::choose_another,
                [this]() {
                    show_welcome();
                },
                ui_text::back,
                [this]() {
                    show_home();
                });
            return;
        }

        if (wrong_edition(release)) {
            return;
        }

        const FileManifest* manifest = manifest_for(release);

        if (manifest == nullptr) {
            show_problem(
                {ui_text::region_title, QString(ui_text::region_body).arg(QString::fromStdString(release.display_name)), QString()}, ui_text::choose_another,
                [this]() {
                    show_welcome();
                },
                ui_text::back,
                [this]() {
                    show_home();
                });
            return;
        }

        Installation installation;
        installation.kind = Installation::Kind::External;
        installation.data_path = path;
        installation.release_id = release.id;
        installation.manifest_sha256 = manifest->aggregate_sha256();
        save_snapshot(result->snapshot, paths_.snapshot_file(installation.data_path, installation.manifest_sha256));
        activate(installation);
    };

    const ReleaseCatalog* catalog = &catalog_;
    task_->start([result, path, catalog](const Progress& progress) {
        *result = FolderVerifier(*catalog).verify(path, progress);
    });
}

bool LauncherWindow::change_games_root(QWidget* parent) {
    const QString chosen = LauncherDialogs(*services_.dialogs).choose_games_root(parent, AppPaths::to_text(games_root()));

    if (chosen.isEmpty()) {
        return false;
    }

    const std::filesystem::path root = AppPaths::to_path(chosen);

    if (!writable_directory(root)) {
        set_banner(QString(ui_text::destination_body).arg(chosen));
        return false;
    }

    const std::filesystem::path previous = settings_.games_root;
    settings_.games_root = root;

    if (!save_settings_or_warn()) {
        settings_.games_root = previous;
        return false;
    }

    set_banner(QString());
    welcome_->set_destination(AppPaths::to_text(root));
    return true;
}

std::filesystem::path LauncherWindow::games_root() const {
    return settings_.games_root.empty() ? paths_.default_games_root() : settings_.games_root;
}

const ReleaseCatalog::Release* LauncherWindow::active_release() const {
    return settings_.active_installation ? catalog_.by_id(settings_.active_installation->release_id) : nullptr;
}

const FileManifest* LauncherWindow::manifest_for(const ReleaseCatalog::Release& release) {
    const auto cached = manifests_.find(release.id);

    if (cached != manifests_.end()) {
        return &cached->second;
    }

    FileManifest manifest;

    if (release.manifest.empty() || !manifest.load(release.manifest, nullptr) || manifest.release_id != release.id ||
        manifest.parent_raw_sha1 != release.raw_sha1) {
        return nullptr;
    }

    return &manifests_.emplace(release.id, std::move(manifest)).first->second;
}

void LauncherWindow::activate(const Installation& installation) {
    const std::optional<Installation> previous = settings_.active_installation;
    const std::map<std::string, Installation> previous_set_up = settings_.installations;
    settings_.active_installation = installation;
    settings_.installations[installation.release_id] = installation;
    expected_release_.clear();
    QString error;

    if (!save_settings(&error)) {
        settings_.active_installation = previous;
        settings_.installations = previous_set_up;
        show_problem(
            {ui_text::settings_save_failed, error, QString()}, ui_text::retry,
            [this, installation]() {
                activate(installation);
            },
            ui_text::back,
            [this]() {
                show_home();
            });
        return;
    }

    show_home();
}

bool LauncherWindow::switch_version(const std::string& release_id) {
    const auto found = settings_.installations.find(release_id);

    if (found == settings_.installations.end() || game_running()) {
        return false;
    }

    const std::optional<Installation> previous = settings_.active_installation;
    settings_.active_installation = found->second;

    if (!save_settings_or_warn()) {
        settings_.active_installation = previous;
        return false;
    }

    return true;
}

void LauncherWindow::set_up_edition(const std::string& release_id) {
    expected_release_ = release_id;
    show_welcome();
}

bool LauncherWindow::wrong_edition(const ReleaseCatalog::Release& release) {
    const ReleaseCatalog::Release* expected = expected_release_.empty() ? nullptr : catalog_.by_id(expected_release_);

    if (expected == nullptr || expected->id == release.id) {
        return false;
    }

    show_problem(
        {ui_text::wrong_edition_title,
         QString(ui_text::wrong_edition_body).arg(QString::fromStdString(release.display_name), QString::fromStdString(expected->display_name)), QString()},
        ui_text::choose_another,
        [this]() {
            show_welcome();
        },
        ui_text::back,
        [this]() {
            show_home();
        });
    return true;
}

void LauncherWindow::refresh_ready() {
    const ReleaseCatalog::Release* release = active_release();

    if (release != nullptr) {
        ready_->set_edition(QString::fromStdString(release->display_name));
        ready_->set_save_folder(AppPaths::to_text(save_root()));
        ready_->set_remaster_status(remaster_status());
    }
}

}
