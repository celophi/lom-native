#include "window/launcher_window.h"
#include "lom/launcher/common/files_provider.h"
#include "services/launcher_dialogs.h"

#include "dialogs/card_import_dialog.h"
#include "lom/launcher/saves/save_folder.h"
#include "pages/ready_page.h"
#include "services/game_process.h"
#include "window/ui_text.h"

#include <QDialog>
#include <QMessageBox>

#include <filesystem>
#include <system_error>

namespace lom::launcher {

void LauncherWindow::browse_saves() {
    if (game_->running()) {
        set_banner(ui_text::busy_body);
        return;
    }

    const QString chosen = LauncherDialogs(*services_.dialogs).choose_save_folder(this, AppPaths::to_text(save_root()));

    if (!chosen.isEmpty()) {
        use_save_root(AppPaths::to_path(chosen));
    }
}

void LauncherWindow::use_save_root(const std::filesystem::path& root) {
    const ReleaseCatalog::Release* release = active_release();

    if (release == nullptr) {
        return;
    }

    const SaveFolder::Inspection inspection = SaveFolder::inspect(root, catalog_, release->id);

    switch (inspection.status) {
    case SaveFolder::Status::Ready:
    case SaveFolder::Status::Empty:
        break;

    case SaveFolder::Status::SlotDirectorySelected:
        show_problem(
            {ui_text::save_slot_title, QString(ui_text::save_slot_body).arg(AppPaths::to_text(inspection.suggested_root)), QString()},
            ui_text::save_folder_label,
            [this, parent = inspection.suggested_root]() {
                use_save_root(parent);
            },
            ui_text::back,
            [this]() {
                show_home();
            });
        return;

    case SaveFolder::Status::OtherRegion:
    case SaveFolder::Status::NotWritable:
    case SaveFolder::Status::GameCannotRead:
    case SaveFolder::Status::Missing:
    case SaveFolder::Status::NotADirectory:
    case SaveFolder::Status::Unreadable: {
        QString reason = QString::fromStdString(inspection.error);

        if (inspection.status == SaveFolder::Status::OtherRegion) {
            reason = ui_text::save_other_region;
        } else if (inspection.status == SaveFolder::Status::NotWritable) {
            reason = ui_text::save_not_writable;
        } else if (inspection.status == SaveFolder::Status::GameCannotRead) {
            reason = QString(ui_text::save_game_cannot_read).arg(reason);
        }

        show_problem(
            {ui_text::save_problem_title, reason + QStringLiteral("\n\n") + ui_text::save_kept, AppPaths::to_text(root)}, ui_text::choose_folder,
            [this]() {
                show_home();
                browse_saves();
            },
            ui_text::back,
            [this]() {
                show_home();
            });
        return;
    }
    }

    const std::filesystem::path previous = settings_.save_root(release->id);
    settings_.save_roots[release->id] = root;

    if (!save_settings_or_warn()) {
        if (previous.empty()) {
            settings_.save_roots.erase(release->id);
        } else {
            settings_.save_roots[release->id] = previous;
        }
    }

    show_home();
}

void LauncherWindow::open_saves() {
    const std::filesystem::path root = save_root();
    const ReleaseCatalog::Release* release = active_release();
    std::error_code code;

    if (release != nullptr && !files_provider().exists(root, code) && root == paths_.default_save_root(release->id)) {
        SaveFolder::initialize(root, nullptr);
    }

    services_.desktop->open_folder(AppPaths::to_text(root));
}

void LauncherWindow::import_card() {
    const ReleaseCatalog::Release* release = active_release();

    if (release == nullptr || game_->running()) {
        return;
    }

    const QString chosen = LauncherDialogs(*services_.dialogs).choose_memory_card(this);

    if (chosen.isEmpty()) {
        return;
    }

    MemoryCard card;

    if (card.load(AppPaths::to_path(chosen)) != MemoryCard::Status::Ok) {
        QMessageBox::warning(this, ui_text::card_unsupported_title, QString::fromStdString(card.error()));
        return;
    }

    const std::filesystem::path root = save_root();
    CardImportDialog dialog(card, catalog_, release->id, root, *services_.clock, *services_.random, this);

    if (dialog.save_count() == 0) {
        QMessageBox::information(this, ui_text::card_import_title, ui_text::card_no_saves);
        return;
    }

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const SaveImporter::Result& result = dialog.result();

    if (result.status == SaveImporter::Result::Status::Imported) {
        ready_->set_status(QString(ui_text::card_imported).arg(result.imported).arg(AppPaths::to_text(root)));

        if (settings_.save_root(release->id).empty()) {
            settings_.save_roots[release->id] = root;
            save_settings_or_warn();
        }
    } else {
        ready_->set_status(ui_text::card_nothing);
    }
}

std::filesystem::path LauncherWindow::save_root() const {
    const ReleaseCatalog::Release* release = active_release();

    if (release == nullptr) {
        return {};
    }

    const std::filesystem::path chosen = settings_.save_root(release->id);
    return chosen.empty() ? paths_.default_save_root(release->id) : chosen;
}

}
