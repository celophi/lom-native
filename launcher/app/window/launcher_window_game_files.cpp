#include "window/launcher_window.h"

#include "dialogs/game_files_dialog.h"
#include "lom/launcher/common/remaster_install.h"
#include "pages/ready_page.h"
#include "window/ui_text.h"

#include <QDir>

#include <filesystem>
#include <vector>

namespace lom::launcher {

namespace {
/** Where Steam is: LOM_STEAM_ROOT when it's set (the tests use it), otherwise the usual places. */
std::vector<std::filesystem::path> steam_roots() {
    const QString chosen = qEnvironmentVariable("LOM_STEAM_ROOT");

    if (!chosen.isEmpty()) {
        return {AppPaths::to_path(chosen)};
    }

    return RemasterInstall::default_steam_roots(AppPaths::to_path(QDir::homePath()));
}
}

std::filesystem::path LauncherWindow::remaster_folder() const {
    if (!settings_.use_remaster) {
        return {};
    }

    if (!settings_.remaster_root.empty()) {
        return RemasterInstall::looks_like(settings_.remaster_root) ? settings_.remaster_root : std::filesystem::path();
    }

    return RemasterInstall::find(steam_roots()).value_or(std::filesystem::path());
}

QString LauncherWindow::remaster_status() const {
    if (!remaster_folder().empty()) {
        return ui_text::remaster_in_use;
    }

    return settings_.use_remaster ? QString(ui_text::remaster_hint) : QString();
}

void LauncherWindow::show_remaster(GameFilesDialog& dialog) const {
    const std::filesystem::path chosen = settings_.remaster_root;
    QString where;

    if (!chosen.empty()) {
        where = RemasterInstall::looks_like(chosen) ? QString(ui_text::remaster_chosen).arg(AppPaths::to_text(chosen))
                                                    : QString(ui_text::remaster_not_it).arg(AppPaths::to_text(chosen));
    } else if (const auto found = RemasterInstall::find(steam_roots())) {
        where = QString(ui_text::remaster_found).arg(AppPaths::to_text(*found));
    } else {
        where = ui_text::remaster_not_found;
    }

    dialog.set_remaster(settings_.use_remaster, where);
}

void LauncherWindow::open_game_files() {
    const ReleaseCatalog::Release* release = settings_.active_installation ? active_release() : nullptr;
    std::vector<GameFilesEdition> editions;

    for (const ReleaseCatalog::Release& edition : catalog_.releases()) {
        if (edition.runtime.empty()) {
            continue;
        }

        const auto set_up = settings_.installations.find(edition.id);
        editions.push_back({QString::fromStdString(edition.id), QString::fromStdString(edition.display_name),
                            set_up == settings_.installations.end() ? QString() : AppPaths::to_text(set_up->second.data_path)});
    }

    GameFilesDialog dialog(editions, release ? QString::fromStdString(release->id) : QString(), AppPaths::to_text(games_root()), game_running(), this);
    show_remaster(dialog);
    enum class Then { Nothing, SetUp, Verify };
    Then then = Then::Nothing;
    std::string chosen;
    bool switched = false;

    connect(&dialog, &GameFilesDialog::version_chosen, this, [this, &switched](const QString& id) {
        switched = switch_version(id.toStdString()) || switched;
    });
    connect(&dialog, &GameFilesDialog::set_up, this, [&dialog, &then, &chosen](const QString& id) {
        chosen = id.toStdString();
        then = Then::SetUp;
        dialog.accept();
    });
    connect(&dialog, &GameFilesDialog::show_game_files, this, [this](const QString& id) {
        const auto set_up = settings_.installations.find(id.toStdString());

        if (set_up != settings_.installations.end()) {
            services_.desktop->open_folder(AppPaths::to_text(set_up->second.data_path));
        }
    });
    connect(&dialog, &GameFilesDialog::verify_game_files, this, [&dialog, &then]() {
        then = Then::Verify;
        dialog.accept();
    });
    connect(&dialog, &GameFilesDialog::change_games_root, this, [this, &dialog]() {
        if (change_games_root(&dialog)) {
            dialog.set_games_root(AppPaths::to_text(games_root()));
        }
    });
    connect(&dialog, &GameFilesDialog::use_remaster_changed, this, [this](bool enabled) {
        settings_.use_remaster = enabled;
        save_settings_or_warn();
    });
    connect(&dialog, &GameFilesDialog::choose_remaster, this, [this, &dialog]() {
        const QString start = AppPaths::to_text(remaster_folder());
        const QString chosen = services_.dialogs->choose_remaster_folder(&dialog, start);

        if (chosen.isEmpty()) {
            return;
        }

        settings_.remaster_root = AppPaths::to_path(chosen);
        save_settings_or_warn();
        show_remaster(dialog);
    });
    connect(&dialog, &GameFilesDialog::find_remaster, this, [this, &dialog]() {
        settings_.remaster_root.clear();
        save_settings_or_warn();
        show_remaster(dialog);
    });
    dialog.exec();

    switch (then) {
    case Then::SetUp:
        set_up_edition(chosen);
        break;
    case Then::Verify:
        validate_installation(true);
        break;
    case Then::Nothing:
        if (switched) {
            validate_installation(false);
        } else {
            ready_->set_remaster_status(remaster_status());
        }

        break;
    }
}

}
