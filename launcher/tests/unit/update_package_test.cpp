#include "lom/launcher/common/update_package.h"
#include "../support/disk.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

using namespace lom::launcher;
namespace disk = lom::launcher::test::disk;

constexpr char digest_a[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char digest_b[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

/**
 * @brief Write a small text file, making its folder.
 * @param path The file.
 * @param text What goes in it.
 */
void write(const std::filesystem::path& path, const std::string& text) {
    disk::create_directories(path.parent_path());
    *disk::write(path, std::ios::binary) << text;
}

/**
 * @brief Read a text file.
 * @param path The file.
 * @return What's in it, or empty when it's missing.
 */
std::string read(const std::filesystem::path& path) {
    const auto in_file = disk::read(path, std::ios::binary);
    std::istream& in = *in_file;
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

class UpdatePackageFiles : public ::testing::Test {
  protected:
    /**
     * @brief Make an empty folder for the test.
     */
    void SetUp() override {
        root = std::filesystem::temp_directory_path() /
               ("lom-update-package-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        disk::remove_all(root);
        disk::create_directories(root);
    }

    /**
     * @brief Delete the test's folder.
     */
    void TearDown() override {
        disk::remove_all(root);
    }

    std::filesystem::path root;
};

}

TEST(UpdatePackage, names_each_kind_of_release_file_as_packaging_builds_it) {
    EXPECT_EQ(UpdatePackage::asset_name(UpdatePackage::Kind::AppImage, "1.0.1"), "lom-native-1.0.1-linux-x86_64.AppImage");
    EXPECT_EQ(UpdatePackage::asset_name(UpdatePackage::Kind::WindowsFolder, "1.0.1"), "lom-native-1.0.1-windows-x64.zip");
    EXPECT_EQ(UpdatePackage::asset_name(UpdatePackage::Kind::MacBundle, "1.0.1"), "lom-native-1.0.1-macos-arm64.dmg");
}

TEST(UpdatePackage, finds_a_files_checksum_in_sha256sums) {
    const std::string sums = std::string(digest_a) + "  ffmpeg-9.0.2.tar.xz\n" + digest_b + "  lom-native-1.0.1-windows-x64.zip\n";

    EXPECT_EQ(UpdatePackage::checksum(sums, "lom-native-1.0.1-windows-x64.zip"), std::string(digest_b));
}

TEST(UpdatePackage, reads_binary_mode_lines_and_uppercase_digests) {
    std::string upper = digest_b;

    for (char& c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    EXPECT_EQ(UpdatePackage::checksum(upper + " *file.zip\r\n", "file.zip"), std::string(digest_b));
}

TEST(UpdatePackage, has_no_checksum_for_a_file_that_isnt_listed_or_a_malformed_line) {
    EXPECT_EQ(UpdatePackage::checksum(std::string(digest_a) + "  other.zip\n", "file.zip"), std::nullopt);
    EXPECT_EQ(UpdatePackage::checksum("nothex  file.zip\n", "file.zip"), std::nullopt);
    EXPECT_EQ(UpdatePackage::checksum("", "file.zip"), std::nullopt);
}

TEST_F(UpdatePackageFiles, replaces_a_file_and_removes_the_old_one) {
    write(root / "app.AppImage", "old");
    write(root / "new.AppImage", "new");
    std::string error;

    ASSERT_TRUE(UpdatePackage::replace_file(root / "app.AppImage", root / "new.AppImage", &error)) << error;

    EXPECT_EQ(read(root / "app.AppImage"), "new");
    EXPECT_FALSE(disk::exists(root / "new.AppImage"));
    EXPECT_FALSE(disk::exists(root / "app.AppImage.old-update"));
}

TEST_F(UpdatePackageFiles, keeps_the_old_file_when_the_new_one_is_missing) {
    write(root / "app.AppImage", "old");
    std::string error;

    EXPECT_FALSE(UpdatePackage::replace_file(root / "app.AppImage", root / "missing.AppImage", &error));

    EXPECT_EQ(read(root / "app.AppImage"), "old");
    EXPECT_FALSE(error.empty());
}

TEST_F(UpdatePackageFiles, replaces_a_folders_files_keeping_the_old_ones_aside_and_files_only_it_has) {
    write(root / "install" / "Legend of Mana Native.exe", "old launcher");
    write(root / "install" / "licenses" / "a.txt", "old licence");
    write(root / "install" / "removed.dll", "only in the old version");
    write(root / "new" / "Legend of Mana Native.exe", "new launcher");
    write(root / "new" / "licenses" / "a.txt", "new licence");
    write(root / "new" / "added.dll", "only in the new version");
    std::string error;

    ASSERT_TRUE(UpdatePackage::replace_folder(root / "install", root / "new", &error)) << error;

    EXPECT_EQ(read(root / "install" / "Legend of Mana Native.exe"), "new launcher");
    EXPECT_EQ(read(root / "install" / "licenses" / "a.txt"), "new licence");
    EXPECT_EQ(read(root / "install" / "added.dll"), "only in the new version");
    EXPECT_EQ(read(root / "install" / "removed.dll"), "only in the old version");
    EXPECT_EQ(read(root / "install" / "Legend of Mana Native.exe.old-update"), "old launcher");
}

TEST_F(UpdatePackageFiles, puts_a_folder_back_when_a_new_file_cant_go_in) {
    write(root / "install" / "a.exe", "old a");
    write(root / "install" / "blocked", "a file where the new version has a folder");
    write(root / "new" / "a.exe", "new a");
    write(root / "new" / "blocked" / "inside.txt", "can't go in, since blocked is a file");
    std::string error;

    EXPECT_FALSE(UpdatePackage::replace_folder(root / "install", root / "new", &error));

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(read(root / "install" / "a.exe"), "old a");
    EXPECT_EQ(read(root / "install" / "blocked"), "a file where the new version has a folder");
    EXPECT_FALSE(disk::exists(root / "install" / "a.exe.old-update"));
}

TEST_F(UpdatePackageFiles, replaces_a_bundle_keeping_the_old_one_beside_it) {
    write(root / "Legend of Mana Native.app" / "Contents" / "Info.plist", "old");
    write(root / "staged" / "Legend of Mana Native.app" / "Contents" / "Info.plist", "new");
    std::string error;

    ASSERT_TRUE(UpdatePackage::replace_bundle(root / "Legend of Mana Native.app", root / "staged" / "Legend of Mana Native.app", &error)) << error;

    EXPECT_EQ(read(root / "Legend of Mana Native.app" / "Contents" / "Info.plist"), "new");
    EXPECT_EQ(read(root / "Legend of Mana Native.app.old-update" / "Contents" / "Info.plist"), "old");
}

TEST_F(UpdatePackageFiles, removes_what_an_update_left_beside_and_inside_the_target) {
    write(root / "Legend of Mana Native.app.old-update" / "Contents" / "Info.plist", "old bundle");
    write(root / "Legend of Mana Native.app" / "Contents" / "Info.plist", "new bundle");
    write(root / "Legend of Mana Native.app" / "Contents" / "MacOS" / "game.old-update", "old file");

    UpdatePackage::remove_leftovers(root / "Legend of Mana Native.app");

    EXPECT_FALSE(disk::exists(root / "Legend of Mana Native.app.old-update"));
    EXPECT_FALSE(disk::exists(root / "Legend of Mana Native.app" / "Contents" / "MacOS" / "game.old-update"));
    EXPECT_EQ(read(root / "Legend of Mana Native.app" / "Contents" / "Info.plist"), "new bundle");
}
