#include "dialogs/settings_dialog.h"

#include "window/theme.h"
#include "window/ui_text.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace lom::launcher {

namespace {

constexpr int dialog_width = 460;

}

SettingsDialog::SettingsDialog(LauncherSettings::Theme theme, bool check_updates, bool record_sessions, QWidget* parent) : QDialog(parent) {
    setWindowTitle(ui_text::settings_title);
    setMinimumWidth(dialog_width);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    language_ = new QComboBox(this);
    language_->setObjectName("languageBox");
    language_->addItem(ui_text::language_system, QString());

    for (const ui_text::Language& language : ui_text::languages()) {
        language_->addItem(QString::fromUtf8(language.name), QString::fromLatin1(language.code));
    }

    form->addRow(ui_text::language_label, language_);

    auto* theme_box = new QComboBox(this);
    theme_box->addItem(ui_text::theme_system, int(LauncherSettings::Theme::System));
    theme_box->addItem(ui_text::theme_light, int(LauncherSettings::Theme::Light));
    theme_box->addItem(ui_text::theme_dark, int(LauncherSettings::Theme::Dark));
    theme_box->setCurrentIndex(theme_box->findData(int(theme)));
    form->addRow(ui_text::theme_label, theme_box);

    auto* updates = new QCheckBox(ui_text::check_updates_on_startup, this);
    updates->setChecked(check_updates);
    form->addRow(QString(), updates);
    auto* record = new QCheckBox(ui_text::record_sessions, this);
    record->setObjectName("recordSessionsCheck");
    record->setChecked(record_sessions);
    form->addRow(QString(), record);
    auto* record_note = new QLabel(ui_text::record_sessions_note, this);
    record_note->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    Theme::set_role(record_note, Theme::muted_label);
    record_note->setWordWrap(true);
    form->addRow(QString(), record_note);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(ui_text::close);
    layout->addWidget(buttons);

    connect(language_, &QComboBox::currentIndexChanged, this, [this]() {
        emit language_changed(language_->currentData().toString());
    });
    connect(theme_box, &QComboBox::currentIndexChanged, this, [this, theme_box]() {
        emit theme_changed(LauncherSettings::Theme(theme_box->currentData().toInt()));
    });
    connect(updates, &QCheckBox::toggled, this, &SettingsDialog::check_updates_changed);
    connect(record, &QCheckBox::toggled, this, &SettingsDialog::record_sessions_changed);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    const int needed = layout()->totalHeightForWidth(width());

    if (needed > height()) {
        resize(width(), needed);
    }
}

void SettingsDialog::set_language(const QString& code) {
    const QSignalBlocker blocker(language_);
    const int index = language_->findData(code);
    language_->setCurrentIndex(index < 0 ? 0 : index);
}

}
