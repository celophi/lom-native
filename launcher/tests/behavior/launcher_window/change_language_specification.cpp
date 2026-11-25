#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "window/ui_text.h"
#include "spec.h"
#include "spec_printers.h"

#include <QComboBox>
#include <QDialog>
#include <QLocale>
#include <QRegularExpression>

#include <map>

using namespace lom::launcher::test;

namespace {
struct ChangeLanguageSpecification : LauncherSpec {
    /**
     * @brief Set up the game.
     */
    ChangeLanguageSpecification() {
        folder = given_an_extracted_folder("Legend of Mana");
        given_the_game_is_set_up_from(folder);
    }

    /**
     * @brief Put the system back in English for the specifications after this one.
     */
    ~ChangeLanguageSpecification() override {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    /**
     * @brief Choose a language in the settings, then close them.
     * @param code The language's code.
     */
    void choose_language(const QString& code) {
        given_the_next_dialog_is_answered([code](QWidget* dialog) {
            auto* box = dialog->findChild<QComboBox*>("languageBox");
            ASSERT_NE(box, nullptr);
            box->setCurrentIndex(box->findData(code));
            static_cast<QDialog*>(dialog)->reject();
        });
        press("settingsButton");
    }

    std::filesystem::path folder;
};

/**
 * @brief The %1, %2... a text fills in.
 * @param text Text.
 * @return Each one, with how many times it's there.
 */
std::map<QString, int> placeholders(const QString& text) {
    std::map<QString, int> found;
    QRegularExpressionMatchIterator it = QRegularExpression(QStringLiteral("%\\d")).globalMatch(text);

    while (it.hasNext()) {
        found[it.next().captured()]++;
    }

    return found;
}

}

LOM_SPEC(ChangeLanguageSpecification, "Choosing German should remember it and build the launcher again in German") {
    open_launcher();
    int rebuilds = 0;
    QObject::connect(&launcher(), &lom::launcher::LauncherWindow::rebuild_requested, [&rebuilds]() {
        rebuilds++;
    });

    choose_language(QStringLiteral("de"));

    EXPECT_EQ(settings_member("language").as_string(), "de");
    EXPECT_EQ(rebuilds, 1);
}

LOM_SPEC(ChangeLanguageSpecification, "Opening the launcher with German chosen should show it in German") {
    given_the_game_is_set_up_from(folder, R"("language": "de")");

    open_launcher();

    EXPECT_EQ(heading(), "Bereit zum Spielen");
}

LOM_SPEC(ChangeLanguageSpecification, "Opening the launcher with no language chosen should follow the system") {
    QLocale::setDefault(QLocale(QLocale::Japanese, QLocale::Japan));

    open_launcher();

    EXPECT_EQ(heading(), QString::fromUtf8("\xe6\xba\x96\xe5\x82\x99\xe5\xae\x8c\xe4\xba\x86"));
}

LOM_SPEC(ChangeLanguageSpecification, "Choosing the language the launcher is already in should not build it again") {
    open_launcher();
    int rebuilds = 0;
    QObject::connect(&launcher(), &lom::launcher::LauncherWindow::rebuild_requested, [&rebuilds]() {
        rebuilds++;
    });

    choose_language(QStringLiteral("en"));

    EXPECT_EQ(settings_member("language").as_string(), "en");
    EXPECT_EQ(rebuilds, 0);
}

LOM_SPEC(ChangeLanguageSpecification, "Every language should have all of the launcher's text, with the same blanks to fill in") {
    for (const lom::launcher::ui_text::Language& language : lom::launcher::ui_text::languages()) {
        const QString code = QString::fromLatin1(language.code);

        if (code == QLatin1String("en")) {
            continue;
        }

        std::map<QString, QString> texts;

        for (const auto& [key, text] : lom::launcher::ui_text::translation(code)) {
            texts[key] = text;
        }

        for (const lom::launcher::ui_text::Text* text : lom::launcher::ui_text::all()) {
            const QString key = QString::fromLatin1(text->key());

            if (key == QLatin1String("window_title")) {
                continue;
            }

            const auto found = texts.find(key);
            EXPECT_TRUE(found != texts.end() && !found->second.isEmpty()) << code.toStdString() << " has no " << key.toStdString();

            if (found != texts.end()) {
                EXPECT_EQ(placeholders(found->second), placeholders(QString::fromUtf8(text->english()))) << code.toStdString() << " " << key.toStdString();
                texts.erase(found);
            }
        }

        for (const auto& [key, unused] : texts) {
            ADD_FAILURE() << code.toStdString() << " has " << key.toStdString() << ", which the launcher doesn't use";
        }
    }
}

LOM_SPEC(ChangeLanguageSpecification, "Following the system should use the first of its languages the launcher has") {
    using lom::launcher::ui_text::language_for;

    EXPECT_EQ(language_for({QStringLiteral("pt-BR"), QStringLiteral("fr-CA"), QStringLiteral("en-US")}), "fr");
    EXPECT_EQ(language_for({QStringLiteral("de_DE")}), "de");
    EXPECT_EQ(language_for({QStringLiteral("ko-KR"), QStringLiteral("ja-JP")}), "ko");

    EXPECT_EQ(language_for({QStringLiteral("zh-Hant-TW")}), "zh_TW");
    EXPECT_EQ(language_for({QStringLiteral("zh-HK")}), "zh_TW");
    EXPECT_EQ(language_for({QStringLiteral("zh-Hans-CN")}), "zh_CN");
    EXPECT_EQ(language_for({QStringLiteral("zh")}), "zh_CN");

    EXPECT_EQ(language_for({QStringLiteral("pt-BR"), QStringLiteral("it-IT")}), "en");
    EXPECT_EQ(language_for({}), "en");
}

LOM_SPEC(ChangeLanguageSpecification, "Playing should tell the game the launcher's language, for its own menu") {
    given_the_game_is_set_up_from(folder, R"("language": "ko")");
    open_launcher();

    press("playButton");

    ASSERT_EQ(game_launches.size(), 1u);
    EXPECT_EQ(game_launches.front().environment.value("LOM_LANGUAGE"), "ko");
}
