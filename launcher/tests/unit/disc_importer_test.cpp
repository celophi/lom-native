#include "../support/disc_fixture.h"
#include "../support/disk.h"
#include "../support/test_harness.h"
#include "lom/launcher/disc/disc_importer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>

namespace {

using namespace lom::launcher;
using namespace lom::launcher::test;
using InstallStatus = DiscImporter::InstallResult::Status;

constexpr std::size_t sha1_hex_length = 40;

/**
 * @brief Count entries in a directory tree.
 * @param path Directory.
 * @return Number of entries, or zero when absent.
 */
std::size_t entry_count(const std::filesystem::path& path) {
    return lom::launcher::test::disk::list(path, true).size();
}

class DiscImporterTest : public ::testing::Test {
  protected:
    DiscFixture fixture = DiscFixture::create(TestHarness::instance().scratch_directory());
    std::filesystem::path games = fixture.directory / "games";
    DiscImporter importer{fixture.catalog};
    DiscImporter::Identification identification = importer.identify(fixture.cue, Progress());
};

TEST_F(DiscImporterTest, a_cancelled_install_leaves_nothing_behind) {
    std::atomic<bool> cancel{true};
    const auto result = importer.install(identification, fixture.manifest, games, Progress({}, &cancel));
    EXPECT_EQ(result.status, InstallStatus::Cancelled);
    EXPECT_EQ(entry_count(games / "us"), 0u);
    EXPECT_EQ(entry_count(games / DiscImporter::staging_directory), 0u);
}

TEST_F(DiscImporterTest, refuses_a_file_list_made_for_another_disc) {
    FileManifest other = fixture.manifest;
    other.parent_raw_sha1 = std::string(sha1_hex_length, 'f');
    const auto result = importer.install(identification, other, games, Progress());
    EXPECT_EQ(result.status, InstallStatus::NoManifest);
    EXPECT_EQ(entry_count(games / "us"), 0u);
}

}
