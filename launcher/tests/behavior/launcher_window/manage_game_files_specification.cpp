#include "dialogs/game_files_dialog.h"
#include "launcher_spec.h"
#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/common/json.h"
#include "pages/ready_page.h"
#include "pages/welcome_page.h"
#include "spec.h"
#include "spec_printers.h"

#include <QCheckBox>
#include <QRadioButton>
#include <QDialog>
#include <QLabel>
#include <QPushButton>

#include <functional>

using namespace lom::launcher;
using namespace lom::launcher::test;

namespace {
struct ManageGameFilesSpecification : LauncherSpec {
    /**
     * @brief Set the game up from an extracted folder.
     */
    ManageGameFilesSpecification() {
        folder = given_an_extracted_folder("Legend of Mana");
    }

    /**
     * @brief Open the Game files window and do something in it, then close it
     *        unless the action already did.
     * @param action What to do with the window.
     */
    void in_the_game_files_window(std::function<void(QDialog* dialog)> action) {
        given_the_next_dialog_is_answered([action](QWidget* dialog) {
            auto* window = static_cast<QDialog*>(dialog);
            action(window);

            if (window->isVisible()) {
                window->reject();
            }
        });
        press(ReadyPage::game_files_name);
    }

    /**
     * @brief Press a button in a dialog by its text.
     * @param dialog The dialog.
     * @param text The button's text.
     */
    static void click(QDialog* dialog, const QString& text) {
        for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == text) {
                button->click();
                return;
            }
        }

        ADD_FAILURE() << "The dialog has no button labelled \"" << text.toStdString() << "\"";
    }

    /**
     * @brief Make the test catalog's Japanese edition playable: a file list
     *        (the same files as the US one, which is all the launcher checks)
     *        and a game program.
     */
    void given_the_japanese_edition_is_supported() {
        FileManifest japanese = edition.manifest;
        japanese.release_id = "jp";
        japanese.parent_raw_sha1 = std::string(40, '0');
        const std::filesystem::path data = edition.catalog_path.parent_path();
        ASSERT_TRUE(AtomicFile::write(data / "japanese.json", japanese.to_json().dump(true), nullptr));
        std::string error;
        std::optional<Json> catalog = Json::parse(*AtomicFile::read(edition.catalog_path, nullptr), &error);
        ASSERT_TRUE(catalog.has_value()) << error;
        Json releases{Json::Array{}};
        for (Json release : catalog->find("releases")->as_array()) {
            if (release.find("id")->as_string() == "jp") {
                release["manifest"] = "japanese.json";
                release["runtime"] = "lom-jp";
            }
            releases.push_back(release);
        }
        (*catalog)["releases"] = releases;
        ASSERT_TRUE(AtomicFile::write(edition.catalog_path, catalog->dump(true), nullptr));
    }

    /**
     * @brief Set up both editions from extracted folders, one of them in use.
     * @param active The release id of the one in use.
     */
    void given_both_editions_are_set_up(const std::string& active) {
        given_the_japanese_edition_is_supported();
        const std::filesystem::path japanese = given_an_extracted_folder("Seiken Densetsu");
        auto installation = [this](const std::filesystem::path& data, const std::string& release) {
            Json value{Json::Object{}};
            value["kind"] = "external";
            value["data_path"] = data.u8string();
            value["release"] = release;
            value["manifest_sha256"] = edition.manifest.aggregate_sha256();
            value["source_sha1"] = "";
            value["installation_id"] = "";
            return value;
        };
        Json settings{Json::Object{}};
        settings["schema"] = 1;
        settings["ownership_notice_acknowledged"] = true;
        settings["theme"] = "light";
        settings["active_installation"] = installation(active == "jp" ? japanese : folder, active);
        settings["installations"]["us"] = installation(folder, "us");
        settings["installations"]["jp"] = installation(japanese, "jp");
        given_settings(settings.dump(true));
    }

    std::filesystem::path folder;
};
}

LOM_SPEC(ManageGameFilesSpecification, "The ready page should say the game uses the Remaster when Steam has it") {
    given_the_remaster_is_in_steam();
    given_the_game_is_set_up_from(folder);
    open_launcher();

    EXPECT_EQ(text_of(ReadyPage::remaster_status_name), "With the Remaster's art, music and text");
}

LOM_SPEC(ManageGameFilesSpecification, "The ready page should suggest linking the Remaster when it isn't found") {
    given_the_game_is_set_up_from(folder);
    open_launcher();

    EXPECT_EQ(text_of(ReadyPage::remaster_status_name), "Own the Legend of Mana Remaster? Link it in Game files.");
}

LOM_SPEC(ManageGameFilesSpecification, "The ready page should say nothing about the Remaster when it's turned off") {
    given_the_remaster_is_in_steam();
    given_the_game_is_set_up_from(folder, R"("use_remaster": false)");
    open_launcher();

    EXPECT_EQ(text_of(ReadyPage::remaster_status_name), QString());
}

LOM_SPEC(ManageGameFilesSpecification, "Game files should show where the Remaster was found and turning it off should be saved") {
    const std::filesystem::path remaster = given_the_remaster_is_in_steam();
    given_the_game_is_set_up_from(folder);
    open_launcher();

    in_the_game_files_window([&remaster](QDialog* dialog) {
        auto* where = dialog->findChild<QLabel*>(GameFilesDialog::remaster_where_name);
        auto* use = dialog->findChild<QCheckBox*>(GameFilesDialog::use_remaster_name);
        ASSERT_NE(where, nullptr);
        ASSERT_NE(use, nullptr);
        EXPECT_EQ(where->text(), "Found in " + shown(remaster));
        EXPECT_TRUE(use->isChecked());
        use->setChecked(false);
    });

    EXPECT_FALSE(settings_member("use_remaster").as_bool(true));
    EXPECT_EQ(text_of(ReadyPage::remaster_status_name), QString());
}

LOM_SPEC(ManageGameFilesSpecification, "Choosing the Remaster's folder in Game files should be saved and used") {
    const std::filesystem::path chosen = profile / "elsewhere" / "Legend of Mana";
    std::filesystem::create_directories(chosen / "windata" / "hdmap");
    remaster_folder_choice = shown(chosen);
    given_the_game_is_set_up_from(folder);
    open_launcher();

    in_the_game_files_window([&chosen](QDialog* dialog) {
        click(dialog, "Choose folder...");
        auto* where = dialog->findChild<QLabel*>(GameFilesDialog::remaster_where_name);
        ASSERT_NE(where, nullptr);
        EXPECT_EQ(where->text(), "Using " + shown(chosen));
    });

    EXPECT_EQ(std::filesystem::u8path(settings_member("remaster_root").as_string()), chosen);
    EXPECT_EQ(text_of(ReadyPage::remaster_status_name), "With the Remaster's art, music and text");
}

LOM_SPEC(ManageGameFilesSpecification, "Settings should only hold the launcher's own preferences") {
    given_the_game_is_set_up_from(folder);
    open_launcher();

    given_the_next_dialog_is_answered([](QWidget* dialog) {
        EXPECT_EQ(dialog->findChild<QCheckBox*>(GameFilesDialog::use_remaster_name), nullptr);
        static_cast<QDialog*>(dialog)->reject();
    });
    press(ReadyPage::settings_name);
}

LOM_SPEC(ManageGameFilesSpecification, "Game files should list both versions, the one set up picked and the other not set up") {
    given_the_japanese_edition_is_supported();
    given_the_game_is_set_up_from(folder);
    open_launcher();

    in_the_game_files_window([](QDialog* dialog) {
        auto* us = dialog->findChild<QRadioButton*>(GameFilesDialog::radio_name(QStringLiteral("us")));
        auto* jp = dialog->findChild<QRadioButton*>(GameFilesDialog::radio_name(QStringLiteral("jp")));
        ASSERT_NE(us, nullptr);
        ASSERT_NE(jp, nullptr);
        EXPECT_TRUE(us->isChecked() && us->isEnabled());
        EXPECT_FALSE(jp->isChecked() || jp->isEnabled());
        EXPECT_EQ(dialog->findChild<QLabel*>(GameFilesDialog::status_name(QStringLiteral("jp")))->text(), "Not set up yet.");
        EXPECT_EQ(dialog->findChild<QPushButton*>(GameFilesDialog::set_up_name(QStringLiteral("jp")))->text(), "Set up...");
        EXPECT_EQ(dialog->findChild<QPushButton*>(GameFilesDialog::set_up_name(QStringLiteral("us")))->text(), "Set up again...");
    });
}

LOM_SPEC(ManageGameFilesSpecification, "Picking the other version in Game files should switch to it without closing, keeping both") {
    given_both_editions_are_set_up("us");
    open_launcher();
    EXPECT_EQ(text_of("installedEdition"), "North American version (NTSC-U) installed");

    in_the_game_files_window([](QDialog* dialog) {
        auto* jp = dialog->findChild<QRadioButton*>(GameFilesDialog::radio_name(QStringLiteral("jp")));
        ASSERT_NE(jp, nullptr);
        ASSERT_TRUE(jp->isEnabled());
        EXPECT_EQ(jp->text(), "Play the Japanese version (NTSC-J)");
        jp->click();
        EXPECT_TRUE(dialog->isVisible());
        EXPECT_TRUE(jp->isChecked());
    });

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(text_of("installedEdition"), "Japanese version (NTSC-J) installed");
    EXPECT_EQ(settings_member("active_installation").find("release")->as_string(), "jp");
    EXPECT_NE(settings_member("installations").find("us"), nullptr);
}

LOM_SPEC(ManageGameFilesSpecification, "Setting up a version should ask for its disc and keep the other version's game") {
    given_both_editions_are_set_up("jp");
    open_launcher();
    given_the_next_dialog_is_answered([](QWidget* dialog) {
        dialog->findChild<QPushButton*>(GameFilesDialog::set_up_name(QStringLiteral("us")))->click();
    });
    press(ReadyPage::game_files_name);
    EXPECT_EQ(heading(), "Welcome");
    EXPECT_EQ(text_of(WelcomePage::body_name), "Choose the disc image or extracted folder of the North American version (NTSC-U).");

    press("browseImageButton");

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(settings_member("active_installation").find("release")->as_string(), "us");
    EXPECT_NE(settings_member("installations").find("jp"), nullptr);
    EXPECT_EQ(settings_member("installations").find("us")->find("kind")->as_string(), "managed");
}

LOM_SPEC(ManageGameFilesSpecification, "Setting up a version should refuse the other version's disc") {
    given_the_japanese_edition_is_supported();
    given_the_game_is_set_up_from(folder);
    open_launcher();
    given_the_next_dialog_is_answered([](QWidget* dialog) {
        dialog->findChild<QPushButton*>(GameFilesDialog::set_up_name(QStringLiteral("jp")))->click();
    });
    press(ReadyPage::game_files_name);

    press("browseImageButton");

    EXPECT_EQ(heading(), "That's the other version");
    EXPECT_EQ(settings_member("installations").find("jp"), nullptr);
    EXPECT_EQ(settings_member("active_installation").find("release")->as_string(), "us");
}
