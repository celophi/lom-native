#ifndef LOM_LAUNCHER_SETTINGS_DIALOG_H
#define LOM_LAUNCHER_SETTINGS_DIALOG_H

#include "lom/launcher/common/launcher_settings.h"

#include <QDialog>

class QComboBox;

namespace lom::launcher {
class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    /**
     * @brief Build the dialog.
     * @param theme Current theme preference.
     * @param check_updates Whether startup update checks are enabled.
     * @param record_sessions Whether Play records sessions for bug reports.
     * @param parent Parent widget.
     */
    SettingsDialog(LauncherSettings::Theme theme, bool check_updates, bool record_sessions, QWidget* parent);

    /**
     * @brief Show the launcher's language.
     * @param code Its code, or empty for the system's.
     */
    void set_language(const QString& code);

  signals:
    /**
     * @brief The launcher's language changed.
     * @param code The new language's code, or empty for the system's.
     */
    void language_changed(const QString& code);

    /**
     * @brief The theme preference changed.
     * @param theme New preference.
     */
    void theme_changed(lom::launcher::LauncherSettings::Theme theme);

    /**
     * @brief The startup update check preference changed.
     * @param enabled New value.
     */
    void check_updates_changed(bool enabled);

    /**
     * @brief The session recording preference changed.
     * @param enabled New value.
     */
    void record_sessions_changed(bool enabled);

  protected:
    /**
     * @brief Grow to the height the wrapped text needs once the width is known.
     * @param event Show event.
     */
    void showEvent(QShowEvent* event) override;

  private:
    QComboBox* language_ = nullptr;
};

}

#endif
