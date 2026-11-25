#include "../support/disc_fixture.h"
#include "../support/test_harness.h"
#include "lom/launcher/disc/disc_importer.h"
#include "lom/launcher/disc/file_snapshot.h"
#include "lom/launcher/disc/folder_verifier.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using namespace lom::launcher;
using namespace lom::launcher::test;
using Status = FolderVerifier::Result::Status;

constexpr std::size_t sha256_hex_length = 64;

/**
 * @brief Flip the last byte of a file.
 * @param path File.
 */
void flip_last_byte(const std::filesystem::path& path) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekg(-1, std::ios::end);
    const char byte = char(file.get() ^ 1);
    file.seekp(-1, std::ios::end);
    file.put(byte);
}

class FileSnapshotTest : public ::testing::Test {
  protected:
    /**
     * @brief Extract the edition into a folder and run the first, full check.
     */
    FileSnapshotTest() : folder(extracted_copy("folder")), first(FolderVerifier::check(folder, fixture.manifest, nullptr, Progress())) {
    }

    /**
     * @brief Install the fixture image and copy its data folder.
     * @param name Name of the copy under the fixture directory.
     * @return The copied folder.
     */
    std::filesystem::path extracted_copy(const std::string& name) const {
        const DiscImporter importer(fixture.catalog);
        const auto installed =
            importer.install(importer.identify(fixture.cue, Progress()), fixture.manifest, fixture.directory / ("games-" + name), Progress());
        const std::filesystem::path copy = fixture.directory / name;
        std::filesystem::copy(installed.installation.data_path, copy, std::filesystem::copy_options::recursive);
        return copy;
    }

    DiscFixture fixture = DiscFixture::create(TestHarness::instance().scratch_directory());
    std::filesystem::path folder;
    FolderVerifier::Result first;
};

TEST_F(FileSnapshotTest, a_first_check_hashes_every_file_and_records_each_one) {
    EXPECT_EQ(first.status, Status::Verified);
    EXPECT_EQ(first.hashed_bytes, fixture.manifest.total_bytes());
    EXPECT_EQ(first.snapshot.entries.size(), fixture.manifest.entries.size());
}

TEST_F(FileSnapshotTest, a_saved_snapshot_loads_for_the_same_folder_and_file_list) {
    const std::filesystem::path saved = fixture.directory / "snapshots" / "folder.json";
    ASSERT_TRUE(first.snapshot.save(saved, nullptr));
    const auto loaded = FileSnapshot::load(saved);
    ASSERT_TRUE(loaded);
    EXPECT_TRUE(loaded->applies_to(folder, fixture.manifest.aggregate_sha256()));
    EXPECT_EQ(loaded->entries.size(), first.snapshot.entries.size());
}

TEST_F(FileSnapshotTest, unchanged_files_are_not_hashed_again) {
    const auto again = FolderVerifier::check(folder, fixture.manifest, &first.snapshot, Progress());
    EXPECT_EQ(again.status, Status::Verified);
    EXPECT_EQ(again.hashed_bytes, 0u);
}

TEST_F(FileSnapshotTest, only_a_file_with_a_new_modification_time_is_hashed_again) {
    const std::filesystem::path changed_file = folder / "MOVIE" / "OPEN.STR";
    flip_last_byte(changed_file);
    std::filesystem::last_write_time(changed_file, std::filesystem::last_write_time(changed_file) + std::chrono::hours(1));

    const auto again = FolderVerifier::check(folder, fixture.manifest, &first.snapshot, Progress());
    EXPECT_EQ(again.status, Status::UnknownContents);
    ASSERT_EQ(again.mismatched.size(), 1u);
    EXPECT_EQ(again.mismatched[0], "MOVIE/OPEN.STR");
    EXPECT_EQ(again.hashed_bytes, std::filesystem::file_size(changed_file));
}

TEST_F(FileSnapshotTest, a_missing_file_is_found_without_hashing) {
    std::filesystem::remove(folder / "BIN" / "TINY.BIN");
    const auto again = FolderVerifier::check(folder, fixture.manifest, &first.snapshot, Progress());
    EXPECT_EQ(again.status, Status::UnknownContents);
    EXPECT_EQ(again.missing.size(), 1u);
    EXPECT_EQ(again.hashed_bytes, 0u);
}

TEST_F(FileSnapshotTest, a_snapshot_of_another_folder_is_not_trusted) {
    const std::filesystem::path elsewhere = extracted_copy("elsewhere");
    const auto other = FolderVerifier::check(elsewhere, fixture.manifest, &first.snapshot, Progress());
    EXPECT_EQ(other.status, Status::Verified);
    EXPECT_EQ(other.hashed_bytes, fixture.manifest.total_bytes());
}

TEST_F(FileSnapshotTest, a_snapshot_for_another_file_list_is_not_trusted) {
    FileSnapshot foreign = first.snapshot;
    foreign.manifest_sha256 = std::string(sha256_hex_length, '0');
    const auto again = FolderVerifier::check(folder, fixture.manifest, &foreign, Progress());
    EXPECT_EQ(again.status, Status::Verified);
    EXPECT_EQ(again.hashed_bytes, fixture.manifest.total_bytes());
}

TEST_F(FileSnapshotTest, an_installation_comes_with_a_snapshot_of_its_files) {
    const DiscImporter importer(fixture.catalog);
    const auto installed = importer.install(importer.identify(fixture.cue, Progress()), fixture.manifest, fixture.directory / "games", Progress());
    ASSERT_EQ(installed.status, DiscImporter::InstallResult::Status::Installed);
    ASSERT_TRUE(installed.snapshot.has_value());
    const auto again = FolderVerifier::check(installed.installation.data_path, fixture.manifest, &*installed.snapshot, Progress());
    EXPECT_EQ(again.status, Status::Verified);
    EXPECT_EQ(again.hashed_bytes, 0u);
}

}
