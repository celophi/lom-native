#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <filesystem>

using namespace lom::launcher::test;

namespace {
struct PlaySpecification : LauncherSpec {
    /**
     * @brief Set up the game from an extracted folder.
     */
    PlaySpecification() : folder(given_an_extracted_folder("Legend of Mana")) {
        given_the_game_is_set_up_from(folder);
    }

    std::filesystem::path folder;
};

}

LOM_SPEC(PlaySpecification, "Play should start the game with the game files folder and the save folder") {
    open_launcher();

    press("playButton");

    EXPECT_EQ(game_launches.size(), 1);
    const auto& launch = game_launches.front();
    EXPECT_EQ(launch.program, shown(program_directory / game_program));
    EXPECT_EQ(launch.arguments, QStringList{shown(folder)});
    EXPECT_EQ(launch.environment.size(), 8);
    EXPECT_EQ(launch.environment.value("LOM_CARD_ROOT"), shown(data_directory / "cards"));
    EXPECT_EQ(launch.environment.value("LOM_MODS_DIR"), shown(data_directory / "mods"));
    EXPECT_EQ(launch.environment.value("LOM_BUNDLED_MODS_DIR"), shown(edition.catalog_path.parent_path() / "mods"));
    EXPECT_EQ(launch.environment.value("SDL_APP_ID"), QStringLiteral("io.github.celophi.lom_native"));
    EXPECT_EQ(launch.environment.value("LOM_NO_ERROR_DIALOG"), QStringLiteral("1"));
    EXPECT_EQ(launch.environment.value("LOM_REPORT_PREFIX"), shown(data_directory / "logs" / "game-20261015-120000"));
    EXPECT_TRUE(launch.environment.contains("LOM_REMASTER_ROOT"));
    EXPECT_EQ(launch.environment.value("LOM_REMASTER_ROOT"), QString());
    EXPECT_EQ(launch.environment.value("LOM_LANGUAGE"), "en");
    EXPECT_EQ(launch.working_directory, shown(data_directory));
    EXPECT_EQ(launch.log_file, shown(data_directory / "logs" / "game-20261015-120000.log"));
}

LOM_SPEC(PlaySpecification, "Play should pass the Remaster's folder when Steam has it") {
    const std::filesystem::path remaster = given_the_remaster_is_in_steam();
    open_launcher();

    press("playButton");

    ASSERT_EQ(game_launches.size(), 1);
    EXPECT_EQ(game_launches.front().environment.value("LOM_REMASTER_ROOT"), shown(remaster));
}

LOM_SPEC(PlaySpecification, "Play should not pass the Remaster's folder when it's turned off") {
    given_the_remaster_is_in_steam();
    given_the_game_is_set_up_from(folder, R"("use_remaster": false)");
    open_launcher();

    press("playButton");

    ASSERT_EQ(game_launches.size(), 1);
    EXPECT_EQ(game_launches.front().environment.value("LOM_REMASTER_ROOT"), QString());
}

LOM_SPEC(PlaySpecification, "Play should pass the Remaster folder the player chose over Steam's") {
    given_the_remaster_is_in_steam();
    const std::filesystem::path chosen = std::filesystem::u8path(profile.u8string() + "/elsewhere/Legend of Mana");
    disk::create_directories(chosen / "windata" / "hdmap");
    given_the_game_is_set_up_from(folder, R"("remaster_root": "{profile}/elsewhere/Legend of Mana")");
    open_launcher();

    press("playButton");

    ASSERT_EQ(game_launches.size(), 1);
    EXPECT_EQ(game_launches.front().environment.value("LOM_REMASTER_ROOT"), shown(chosen));
}

LOM_SPEC(PlaySpecification, "Play should hide the launcher while the game runs") {
    open_launcher();

    press("playButton");

    EXPECT_FALSE(launcher().isVisible());
}

LOM_SPEC(PlaySpecification, "Play should explain that the game could not start when the game program is missing") {
    disk::remove(program_directory / game_program);
    open_launcher();

    press("playButton");

    EXPECT_EQ(heading(), "The game could not start");
    EXPECT_THAT(text_of("problemBody"), StartsWithText("The game program was not found next to the launcher."));
    EXPECT_TRUE(launcher().isVisible());
    EXPECT_TRUE(game_launches.empty());
}

LOM_SPEC(PlaySpecification, "Play should explain that another copy of the game is using the save folder when its lock is taken") {
    save_folder_in_use = true;
    open_launcher();

    press("playButton");

    EXPECT_EQ(heading(), "The game could not start");
    EXPECT_THAT(text_of("problemBody"), StartsWithText("Another copy of the game is already using this save folder."));
    EXPECT_TRUE(game_launches.empty());
}

LOM_SPEC(PlaySpecification, "Play should explain that the game could not start when the operating system refuses to run it") {
    game_fails_to_start = true;
    game_start_failure = "Permission denied";
    open_launcher();

    press("playButton");

    EXPECT_EQ(heading(), "The game could not start");
    EXPECT_THAT(text_of("problemBody"), StartsWithText("Permission denied"));
    EXPECT_TRUE(launcher().isVisible());
}

LOM_SPEC(PlaySpecification, "Play should create the default save folder with both memory card slots when it does not exist yet") {
    open_launcher();

    press("playButton");

    EXPECT_TRUE(disk::is_directory(data_directory / "cards" / "slot0"));
    EXPECT_TRUE(disk::is_directory(data_directory / "cards" / "slot1"));
}

LOM_SPEC(PlaySpecification, "Play should explain that the save folder is not available when a chosen save folder is missing") {
    given_settings(std::string(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light",
        "save_roots": {"us": "{profile}/USB drive/saves"},
        "active_installation": {"kind": "external", "data_path": "{profile}/Legend of Mana", "release": "us",
            "manifest_sha256": "{manifest}", "source_sha1": "", "installation_id": ""}})"));
    open_launcher();

    press("playButton");

    EXPECT_EQ(heading(), "Your save folder is not available");
    EXPECT_TRUE(game_launches.empty());
    EXPECT_FALSE(disk::exists(profile / "USB drive"));
}

LOM_SPEC(PlaySpecification, "Play should let the game save in the save folder I chose") {
    disk::create_directories(profile / "My saves");
    given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "save_roots": {"us": "{profile}/My saves"},
        "active_installation": {"kind": "external", "data_path": "{profile}/Legend of Mana", "release": "us",
            "manifest_sha256": "{manifest}", "source_sha1": "", "installation_id": ""}})");
    open_launcher();

    press("playButton");

    ASSERT_EQ(game_launches.size(), 1u);
    EXPECT_EQ(native(game_launches.front().environment.value("LOM_CARD_ROOT")), shown(profile / "My saves"));
}
