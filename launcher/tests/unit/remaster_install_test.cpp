#include "lom/launcher/common/remaster_install.h"
#include "../support/disk.h"
#include "../support/test_harness.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace {

using namespace lom::launcher;
namespace disk = lom::launcher::test::disk;
using lom::launcher::test::TestHarness;

/** A folder that looks like the Remaster's install. */
std::filesystem::path install_remaster(const std::filesystem::path& library) {
    const std::filesystem::path folder = library / "steamapps" / "common" / RemasterInstall::steam_folder;
    disk::create_directories(folder / "windata" / "hdmap");
    return folder;
}

TEST(RemasterInstall, reads_the_library_paths_from_libraryfolders_vdf) {
    const char text[] = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"/home/me/.local/share/Steam\"\n\t\t\"label\"\t\t\"\"\n\t}\n"
                        "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\SteamLibrary\"\n\t\t\"apps\"\n\t\t{\n\t\t}\n\t}\n}\n";
    const auto folders = RemasterInstall::parse_library_folders(text);
    ASSERT_EQ(folders.size(), 2u);
    EXPECT_EQ(folders[0].u8string(), "/home/me/.local/share/Steam");
    EXPECT_EQ(folders[1].u8string(), "D:\\SteamLibrary");
}

TEST(RemasterInstall, finds_it_in_another_library_steam_lists) {
    const std::filesystem::path root = TestHarness::instance().scratch_directory() / "remaster-libraries";
    disk::remove_all(root);
    const std::filesystem::path steam = root / "steam";
    const std::filesystem::path other = root / "other drive";
    disk::create_directories(steam / "steamapps");
    *disk::write(steam / "steamapps" / "libraryfolders.vdf")
        << "\"libraryfolders\"\n{\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" << other.generic_u8string() << "\"\n\t}\n}\n";
    EXPECT_FALSE(RemasterInstall::find({steam}));

    const std::filesystem::path folder = install_remaster(other);
    const auto found = RemasterInstall::find({steam});
    ASSERT_TRUE(found);
    EXPECT_TRUE(disk::equivalent(*found, folder));
}

TEST(RemasterInstall, knows_its_folder_by_the_hd_maps) {
    const std::filesystem::path root = TestHarness::instance().scratch_directory() / "remaster-looks";
    disk::remove_all(root);
    disk::create_directories(root / "empty");
    EXPECT_FALSE(RemasterInstall::looks_like(root / "empty"));
    EXPECT_FALSE(RemasterInstall::looks_like(root / "missing"));
    EXPECT_FALSE(RemasterInstall::looks_like({}));
    const std::filesystem::path folder = install_remaster(root);
    EXPECT_TRUE(RemasterInstall::looks_like(folder));
    EXPECT_TRUE(RemasterInstall::looks_like(folder / "windata"));
}

}
