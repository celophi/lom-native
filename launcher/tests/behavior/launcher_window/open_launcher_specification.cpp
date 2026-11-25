#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <QPushButton>
#include <QToolButton>

#include <filesystem>
#include <vector>
#include <string>
#include <fstream>

using namespace lom::launcher::test;

namespace {
struct OpenLauncherSpecification : LauncherSpec {};

}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should show the ownership notice when this user has never acknowledged it") {
    std::filesystem::remove(config_directory / "launcher.json");

    open_launcher();

    EXPECT_EQ(heading(), "Own the game before you play");
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should show the ownership notice when game files are set up but the notice was never acknowledged") {
    const auto folder = given_an_extracted_folder("Legend of Mana");
    given_the_game_is_set_up_from(folder);
    given_settings(R"({"schema": 1, "ownership_notice_acknowledged": false, "theme": "light"})");

    open_launcher();

    EXPECT_EQ(heading(), "Own the game before you play");
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should ask for a game source when no game files are set up") {
    open_launcher();

    EXPECT_EQ(heading(), "Welcome");
    EXPECT_EQ(text_of("destinationFolder"), shown(data_directory / "games"));
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should show Ready to play when the game files set up earlier are unchanged") {
    given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"));

    open_launcher();

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(text_of("installedEdition"), "US version (NTSC-U) installed");
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should explain that the game files are not available when the extracted folder is missing") {
    const auto folder = given_an_extracted_folder("Legend of Mana");
    given_the_game_is_set_up_from(folder);
    std::filesystem::remove_all(folder);

    open_launcher();

    EXPECT_EQ(heading(), "Your game files are not available");
    EXPECT_THAT(text_of("problemBody"), ContainsText(shown(folder)));
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should explain that the game files have changed when a file in the extracted folder was modified") {
    const auto folder = given_an_extracted_folder("Legend of Mana");
    given_the_game_is_set_up_from(folder);
    std::ofstream(folder / "BIN" / "TINY.BIN", std::ios::binary | std::ios::trunc) << "XY";

    open_launcher();

    EXPECT_EQ(heading(), "Your game files have changed");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("BIN/TINY.BIN"));
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should keep the remembered game files when they fail the check") {
    const auto folder = given_an_extracted_folder("Legend of Mana");
    given_the_game_is_set_up_from(folder);
    std::filesystem::remove_all(folder);

    open_launcher();

    EXPECT_TRUE(json_is(settings_member("active_installation"),
                        R"({"kind": "external", "data_path": "{profile}/Legend of Mana", "release": "us", "manifest_sha256": "{manifest}",
                          "source_sha1": "", "installation_id": ""})"));
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should reset damaged settings and say so when the settings file is unreadable") {
    std::ofstream(config_directory / "launcher.json", std::ios::trunc) << "{ not json";

    open_launcher();

    EXPECT_THAT(text_of("banner"), StartsWithText("Your launcher settings could not be read and were reset"));
    EXPECT_EQ(heading(), "Own the game before you play");
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should check for updates once when update checks are enabled") {
    open_launcher();

    EXPECT_EQ(update_requests, 1);
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should not check for updates when update checks are turned off") {
    given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "check_updates_on_startup": false})");

    open_launcher();

    EXPECT_EQ(update_requests, 0);
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should offer the new version when a newer release exists") {
    update_response.body = R"({"tag_name": "v0.5.0", "html_url": "https://github.com/example/lom/releases/tag/v0.5.0", "body": "New things"})";
    given_the_next_message_is_answered_with("Not now");

    open_launcher();

    ASSERT_FALSE(messages.empty());
    EXPECT_THAT(messages.front(), StartsWithText("Legend of Mana Native 0.5.0 is available. You have 0.4.0."));
    EXPECT_EQ(heading(), "Welcome");
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should say nothing about updates when the release feed is unreachable") {
    update_response.ok = false;
    update_response.error = "Host not found";

    open_launcher();

    EXPECT_TRUE(messages.empty());
    EXPECT_EQ(heading(), "Welcome");
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should use the dark theme when it was chosen before") {
    given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "dark"})");

    open_launcher();

    EXPECT_EQ(launcher().findChild<QToolButton*>("themeToggle")->text(), "Light mode");
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should ask for legal ownership and discourage piracy when the notice was never acknowledged") {
    std::filesystem::remove(config_directory / "launcher.json");

    open_launcher();

    EXPECT_THAT(text_of("noticeBody"), ContainsText("you must have legally purchased and own a supported original PlayStation copy of Legend of Mana"));
    EXPECT_THAT(text_of("noticeBody"), ContainsText("supported NTSC-U or NTSC-J copy"));
    EXPECT_THAT(text_of("noticeBody"), ContainsText("Don't pirate it. Buy the game."));
    EXPECT_EQ(launcher().findChild<QPushButton*>("noticeAcceptButton")->text(), "I legally own the game");
    EXPECT_EQ(launcher().findChild<QPushButton*>("noticeQuitButton")->text(), "Quit");
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should remove temporary files left by an interrupted extraction") {
    const std::filesystem::path leftover = data_directory / "games" / ".staging" / "1792000000-000001" / "data";
    std::filesystem::create_directories(leftover);
    std::ofstream(leftover / "SLUS_999.99") << "partial";

    open_launcher();

    EXPECT_FALSE(std::filesystem::exists(data_directory / "games" / ".staging"));
}

LOM_SPEC(OpenLauncherSpecification, "Opening the launcher should keep a copy of the settings file when it is unreadable") {
    std::ofstream(config_directory / "launcher.json", std::ios::trunc) << "{ not json";

    open_launcher();

    std::vector<std::filesystem::path> kept;

    for (const auto& entry : std::filesystem::directory_iterator(config_directory)) {
        if (entry.path().filename().u8string().rfind("launcher.json.corrupt-", 0) == 0) {
            kept.push_back(entry.path());
        }
    }

    ASSERT_EQ(kept.size(), 1u);
    std::ifstream in(kept.front());
    EXPECT_EQ(std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()), "{ not json");
}
