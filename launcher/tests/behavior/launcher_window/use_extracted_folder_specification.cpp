#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <filesystem>
#include <fstream>

using namespace lom::launcher::test;

namespace {
struct UseExtractedFolderSpecification : LauncherSpec {
    /**
     * @brief Prepare the user's folder and make the picker choose it.
     */
    UseExtractedFolderSpecification() : folder(given_an_extracted_folder("My Legend of Mana")) {
        game_folder_choice = shown(folder);
    }

    std::filesystem::path folder;
};

}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should show Ready to play when every file matches") {
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(text_of("installedEdition"), "North American version (NTSC-U) installed");
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should use it in place without copying it") {
    open_launcher();

    press("useFolderButton");

    EXPECT_TRUE(json_is(settings_member("active_installation"),
                        R"({"kind": "external", "data_path": "{profile}/My Legend of Mana", "release": "us", "manifest_sha256": "{manifest}",
                          "source_sha1": "", "installation_id": ""})"));
    EXPECT_FALSE(disk::exists(data_directory / "games" / "us"));
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should list the changed file when a file differs from the supported edition") {
    std::string movie = disk::contents(folder / "MOVIE" / "OPEN.STR");
    movie[100] = '\x7F';
    *disk::write(folder / "MOVIE" / "OPEN.STR") << movie;
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "Unsupported game version");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("Different contents or size (1):\n  MOVIE/OPEN.STR"));
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should explain that the names differ when files use lower-case names") {
    disk::rename(folder / "BIN" / "FIELD.BIN", folder / "BIN" / "field.bin");
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "These game files can't be used in place");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("BIN/field.bin -> BIN/FIELD.BIN"));
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should explain that it doesn't contain the game when it has no game executable") {
    disk::create_directories(profile / "Holiday Photos");
    *disk::write(profile / "Holiday Photos" / "beach.jpg") << "jpeg";
    game_folder_choice = shown(profile / "Holiday Photos");
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "This folder doesn't contain the game");
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should leave the folder untouched") {
    const auto modified_before = disk::last_write_time(folder / "BIN" / "FIELD.BIN");
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(disk::last_write_time(folder / "BIN" / "FIELD.BIN"), modified_before);
    EXPECT_FALSE(disk::exists(folder / "receipt.json"));
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should ignore files the operating system adds, such as .DS_Store") {
    *disk::write(folder / ".DS_Store") << "finder data";
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "Ready to play");
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should list the missing file when a game file is missing") {
    disk::remove(folder / "BIN" / "TINY.BIN");
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "Unsupported game version");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("Missing (1):\n  BIN/TINY.BIN"));
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should list the extra file when the folder has a file that isn't part of the game") {
    *disk::write(folder / "BIN" / "MOD.BIN") << "mod";
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "Unsupported game version");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("Not part of the game (1):\n  BIN/MOD.BIN"));
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should explain that the names differ when files keep the ;1 version suffix") {
    disk::rename(folder / "BIN" / "FIELD.BIN", folder / "BIN" / "FIELD.BIN;1");
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "These game files can't be used in place");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("BIN/FIELD.BIN;1 -> BIN/FIELD.BIN"));
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should explain that the names differ when a file exists in both upper and lower case") {
    ASSERT_TRUE(disk::copy_file(folder / "BIN" / "TINY.BIN", folder / "BIN" / "tiny.bin"));

    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "These game files can't be used in place");
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should explain that the edition can't be verified yet when it holds the NTSC-J game") {
    disk::create_directories(profile / "Seiken Densetsu");
    *disk::write(profile / "Seiken Densetsu" / "SLPS_999.98") << "executable";
    game_folder_choice = shown(profile / "Seiken Densetsu");
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "This edition can't be verified yet");
    EXPECT_THAT(text_of("problemBody"), StartsWithText("Japanese version (NTSC-J) was recognized"));
}

LOM_SPEC(UseExtractedFolderSpecification,
         "Using an extracted folder should explain that it holds more than one edition when it has both editions' executables") {
    *disk::write(folder / "SLPS_999.98") << "executable";
    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "Unsupported game version");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("the folder contains executables from more than one edition"));
}

LOM_SPEC(UseExtractedFolderSpecification, "Using an extracted folder should explain that it contains links when a file links outside it") {
    *disk::write(profile / "elsewhere.bin") << "x";
    files_provider_fake.add_link(folder / "BIN" / "LINK.BIN", profile / "elsewhere.bin");

    open_launcher();

    press("useFolderButton");

    EXPECT_EQ(heading(), "This folder contains links");
}
