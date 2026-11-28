#include "../../support/disc_fixture.h"
#include "../../support/synthetic_disc.h"
#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <QPushButton>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace lom::launcher::test;

namespace {
struct ChooseDiscImageSpecification : LauncherSpec {
    /**
     * @brief Copy the default BIN with one byte changed, as a modified image.
     * @return The altered image.
     */
    std::filesystem::path an_altered_image() {
        const auto in_file = disk::read(edition.bin, std::ios::binary);
        std::istream& in = *in_file;
        std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        bytes[bytes.size() - 10] ^= 1;
        const std::filesystem::path altered = profile / "Modified Disc.bin";
        disk::write(altered, std::ios::binary)->write(bytes.data(), std::streamsize(bytes.size()));
        return altered;
    }

    /**
     * @brief Write a raw image of another game into the profile.
     * @return The image.
     */
    std::filesystem::path another_games_bin() {
        const char boot[] = "BOOT = cdrom:\\SLES_999.97;1\r\n";
        SyntheticDisc other;
        other.add("SYSTEM.CNF", std::vector<std::uint8_t>(boot, boot + sizeof(boot) - 1));
        other.add("SLES_999.97", {1, 2, 3});
        SyntheticDisc::write(profile / "Other.bin", other.raw());
        return profile / "Other.bin";
    }

    /**
     * @brief Write an ISO into the profile.
     * @param name File name.
     * @param disc Disc contents.
     * @return The image.
     */
    std::filesystem::path an_iso(const std::string& name, const SyntheticDisc& disc) {
        const std::filesystem::path path = profile / name;
        SyntheticDisc::write(path, disc.cooked());
        return path;
    }

    /**
     * @brief Write a CUE sheet beside the edition's BIN.
     * @param text Sheet contents.
     * @return The sheet.
     */
    std::filesystem::path a_cue_sheet(const std::string& text) {
        const std::filesystem::path path = edition.bin.parent_path() / "Edited.cue";
        *disk::write(path, std::ios::binary) << text;
        return path;
    }

    /**
     * @brief Count the files under a folder.
     * @param folder Folder; absent counts as empty.
     * @return Number of regular files.
     */
    static std::size_t files_under(const std::filesystem::path& folder) {
        std::size_t count = 0;

        for (const lom::launcher::FileEntry& entry : disk::list(folder, true)) {
            count += entry.regular ? 1 : 0;
        }

        return count;
    }
};

constexpr std::size_t raw_sector_bytes = 2352;
constexpr std::size_t cooked_sector_bytes = 2048;

constexpr char installed_data[] = "{data}/games/us/{sha1}/1792065600-000001/data";

}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should show Ready to play when a supported NTSC-U CUE is chosen") {
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(text_of("installedEdition"), "North American version (NTSC-U) installed");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should remember the extracted game files as the active game") {
    open_launcher();

    press("browseImageButton");

    EXPECT_TRUE(json_is(read_json(config_directory / "launcher.json"),
                        std::string(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "games_root": "",
                          "last_image_directory": "{images}", "check_updates_on_startup": true, "save_roots": {}, "last_seen_version": "0.4.0",
                          "record_sessions": false, "use_remaster": true, "remaster_root": "", "language": "", "active_installation": {"kind": "managed", "data_path": ")") +
                            installed_data + R"(", "release": "us", "manifest_sha256": "{manifest}", "source_sha1": "{sha1}",
                          "installation_id": "1792065600-000001"}, "installations": {"us": {"kind": "managed", "data_path": ")" +
                            installed_data +
                            R"(", "release": "us", "manifest_sha256": "{manifest}", "source_sha1": "{sha1}", "installation_id": "1792065600-000001"}}})"));
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should record how the game files were verified in a receipt") {
    open_launcher();

    press("browseImageButton");

    EXPECT_TRUE(json_is(read_json(std::filesystem::u8path(expand("{data}/games/us/{sha1}/1792065600-000001/receipt.json"))),
                        R"({"schema": 1, "release": "us", "source_sha1": "{sha1}", "source_layout": "single-track-raw-2352",
                          "manifest_sha256": "{manifest}", "extractor_version": 1, "installation_id": "1792065600-000001",
                          "created_unix_seconds": 1792065600, "verification": "raw-identity"})"));
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should extract every game file with its original contents") {
    open_launcher();

    press("browseImageButton");

    const std::filesystem::path data = std::filesystem::u8path(expand(installed_data));
    EXPECT_EQ(disk::file_size(data / "SYSTEM.CNF"), 38);
    EXPECT_EQ(disk::file_size(data / "SLUS_999.99"), 6000);
    EXPECT_EQ(disk::file_size(data / "BIN" / "FIELD.BIN"), 5000);
    EXPECT_EQ(disk::file_size(data / "BIN" / "TINY.BIN"), 1);
    EXPECT_EQ(disk::file_size(data / "MOVIE" / "OPEN.STR"), 10240);
    const auto field_file = disk::read(data / "BIN" / "FIELD.BIN", std::ios::binary);
    std::istream& field = *field_file;
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(field)), std::istreambuf_iterator<char>());
    EXPECT_EQ(bytes, SyntheticDisc::pattern(5000, 2));
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should accept an ISO when every file matches the supported edition") {
    disc_image_choice = shown(edition.iso);
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(read_json(std::filesystem::u8path(expand("{data}/games/us/{sha1}/1792065600-000001/receipt.json"))).find("verification")->as_string(),
              "file-contents");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should accept a BIN whose bytes differ from the supported one when every file matches") {
    disc_image_choice = shown(an_altered_image());
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(read_json(std::filesystem::u8path(expand("{data}/games/us/{sha1}/1792065600-000001/receipt.json"))).find("verification")->as_string(),
              "file-contents");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should keep no game files when a BIN's file contents differ from the supported edition") {
    std::vector<std::uint8_t> raw = DiscFixture::standard_disc().raw();
    const std::vector<std::uint8_t> field = SyntheticDisc::pattern(DiscFixture::field_bin_bytes, DiscFixture::field_bin_seed);
    const auto found = std::search(raw.begin(), raw.end(), field.begin(), field.begin() + 64);
    ASSERT_NE(found, raw.end());
    found[100] ^= 1;
    SyntheticDisc::write(profile / "Altered.bin", raw);
    disc_image_choice = shown(profile / "Altered.bin");
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "The game files could not be prepared");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("Files that did not match (1):\n  BIN/FIELD.BIN"));
    EXPECT_EQ(files_under(data_directory / "games"), 0u);
}

LOM_SPEC(ChooseDiscImageSpecification,
         "Choosing a disc image should show Unsupported game version with the computed SHA-1 when a BIN of another game is chosen") {
    disc_image_choice = shown(another_games_bin());
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Unsupported game version");
    EXPECT_THAT(text_of("problemBody"), ContainsText("Supported releases: NTSC-U (SLUS-01013) and NTSC-J (SLPS-02170)."));
    EXPECT_THAT(text_of("problemBody"), ContainsText("PAL versions and modified disc images are not supported."));
    EXPECT_THAT(text_of("problemDetails"), ContainsText("Computed SHA-1: "));
    EXPECT_THAT(text_of("problemDetails"), ContainsText(QString::fromStdString("NTSC-U (SLUS-01013): " + edition.raw_sha1)));
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should explain that the edition can't be verified yet when an NTSC-J ISO is chosen") {
    const char boot[] = "BOOT = cdrom:\\SLPS_999.98;1\r\n";
    SyntheticDisc japanese;
    japanese.add("SYSTEM.CNF", std::vector<std::uint8_t>(boot, boot + sizeof(boot) - 1));
    japanese.add("SLPS_999.98", {1, 2, 3});
    SyntheticDisc::write(profile / "Japanese.iso", japanese.cooked());
    disc_image_choice = shown(profile / "Japanese.iso");
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "This edition can't be verified yet");
    EXPECT_THAT(text_of("problemBody"), StartsWithText("Japanese version (NTSC-J) was recognized"));
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should explain that a disc file is missing when the CUE's BIN is not beside it") {
    disk::remove(edition.bin);
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "A disc file is missing");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should stay on the welcome page when the picker is cancelled") {
    disc_image_choice.clear();
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Welcome");
    EXPECT_FALSE(disk::exists(data_directory / "games" / "us"));
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should keep the current game when the new image is unsupported") {
    given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"));
    open_launcher();
    given_the_next_dialog_is_answered([](QWidget* dialog) {
        dialog->findChild<QPushButton*>(QStringLiteral("setUpButton_us"))->click();
    });
    press("gameFilesButton");
    disc_image_choice = shown(another_games_bin());

    press("browseImageButton");

    EXPECT_EQ(heading(), "Unsupported game version");
    EXPECT_EQ(std::filesystem::path(settings_member("active_installation").find("data_path")->as_string()), profile / "Legend of Mana");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should explain that the game files folder can't be used when it is not writable") {
    *disk::write(profile / "not-a-folder") << "x";
    given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "games_root": "{profile}/not-a-folder"})");
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "The game files folder can't be used");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should extract into the folder chosen for game files") {
    given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "games_root": "{profile}/Games drive"})");
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_TRUE(disk::exists(std::filesystem::u8path(expand("{profile}/Games drive/us/{sha1}/1792065600-000001/data/SLUS_999.99"))));
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should accept the BIN file when it is chosen instead of the CUE") {
    disc_image_choice = shown(edition.bin);
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Ready to play");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should show Unsupported game version when the BIN is cut short inside the game files") {
    const auto in_file = disk::read(edition.bin, std::ios::binary);
    std::istream& in = *in_file;
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    bytes.resize(bytes.size() / 2 / raw_sector_bytes * raw_sector_bytes);
    disk::write(profile / "Incomplete.bin", std::ios::binary)->write(bytes.data(), std::streamsize(bytes.size()));
    disc_image_choice = shown(profile / "Incomplete.bin");
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Unsupported game version");
    EXPECT_EQ(files_under(data_directory / "games"), 0u);
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should accept an ISO that another tool padded with empty sectors") {
    std::vector<std::uint8_t> padded = DiscFixture::standard_disc().cooked();
    padded.resize(padded.size() + 4 * cooked_sector_bytes, 0);
    SyntheticDisc::write(profile / "Padded.iso", padded);
    disc_image_choice = shown(profile / "Padded.iso");
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Ready to play");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should name the extra file when an ISO contains a file the supported edition doesn't have") {
    SyntheticDisc modded = DiscFixture::standard_disc();
    modded.add("BIN/MOD.BIN", {1, 2, 3});
    disc_image_choice = shown(an_iso("Modded.iso", modded));
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Unsupported game version");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("Files that differ from the supported edition (1):\n  BIN/MOD.BIN"));
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should show Unsupported game version when an ISO of another game is chosen") {
    SyntheticDisc other;
    other.add("SYSTEM.CNF", {'B'});
    other.add("SLES_000.01", {1});
    disc_image_choice = shown(an_iso("Other Game.iso", other));
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Unsupported game version");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should explain that the file is not a usable disc image when it isn't one") {
    SyntheticDisc::write(profile / "Not a disc.bin", SyntheticDisc::pattern(raw_sector_bytes * 40, 9));
    disc_image_choice = shown(profile / "Not a disc.bin");
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "This is not a usable disc image");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should explain that the disc layout is unsupported when the CUE describes more than one track") {
    disc_image_choice = shown(
        a_cue_sheet("FILE \"" + edition.bin.filename().u8string() + "\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 01 10:00:00\n"));
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Unsupported disc layout");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should explain that the file is not a usable disc image when the CUE can't be read") {
    disc_image_choice = shown(a_cue_sheet(std::string("FILE\0\x01\x02", 7)));
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "This is not a usable disc image");
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should keep no game files when an ISO's file contents differ from the supported edition") {
    std::vector<std::uint8_t> cooked = DiscFixture::standard_disc().cooked();
    const std::vector<std::uint8_t> field = SyntheticDisc::pattern(DiscFixture::field_bin_bytes, DiscFixture::field_bin_seed);
    const auto found = std::search(cooked.begin(), cooked.end(), field.begin(), field.begin() + 64);
    ASSERT_NE(found, cooked.end());
    found[100] ^= 1;
    SyntheticDisc::write(profile / "Altered.iso", cooked);
    disc_image_choice = shown(profile / "Altered.iso");
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "The game files could not be prepared");
    EXPECT_THAT(text_of("problemDetails"), ContainsText("Files that did not match (1):\n  BIN/FIELD.BIN"));
    EXPECT_EQ(files_under(data_directory / "games"), 0u);
}

LOM_SPEC(ChooseDiscImageSpecification, "Choosing a disc image should leave no temporary files in the game files folder") {
    open_launcher();

    press("browseImageButton");

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_FALSE(disk::exists(data_directory / "games" / ".staging"));
}
