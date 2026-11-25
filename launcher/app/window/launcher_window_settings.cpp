#include "window/launcher_window.h"

#include "dialogs/settings_dialog.h"
#include "window/ui_text.h"

namespace lom::launcher {

void LauncherWindow::open_settings() {
    SettingsDialog dialog(settings_.theme, settings_.check_updates_on_startup, settings_.record_sessions, this);
    dialog.set_language(QString::fromStdString(settings_.language));
    connect(&dialog, &SettingsDialog::language_changed, this, [this](const QString& code) {
        settings_.language = code.toStdString();
        save_settings_or_warn();
    });
    connect(&dialog, &SettingsDialog::theme_changed, this, [this](LauncherSettings::Theme theme) {
        settings_.theme = theme;
        apply_theme();
        save_settings_or_warn();
    });
    connect(&dialog, &SettingsDialog::check_updates_changed, this, [this](bool enabled) {
        settings_.check_updates_on_startup = enabled;
        save_settings_or_warn();
    });
    connect(&dialog, &SettingsDialog::record_sessions_changed, this, [this](bool enabled) {
        settings_.record_sessions = enabled;
        save_settings_or_warn();
    });
    dialog.exec();

    const QString chosen = settings_.language.empty() ? ui_text::system_language() : QString::fromStdString(settings_.language);

    if (chosen != ui_text::language() && !game_running()) {
        emit rebuild_requested();
    }
}

}
