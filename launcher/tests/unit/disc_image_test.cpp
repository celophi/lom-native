#include "../support/disc_fixture.h"
#include "../support/disk.h"
#include "../support/synthetic_disc.h"
#include "../support/test_harness.h"
#include "lom/launcher/disc/disc_image.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

using namespace lom::launcher;
using namespace lom::launcher::test;

constexpr std::size_t raw_sector_bytes = 2352;

constexpr std::size_t noise_sectors = 40;

constexpr std::uint32_t noise_seed = 9;

class DiscImageTest : public ::testing::Test {
  protected:
    DiscFixture fixture = DiscFixture::create(TestHarness::instance().scratch_directory());
    DiscImage image;
};

TEST_F(DiscImageTest, reads_a_bin_as_raw_sectors) {
    EXPECT_EQ(image.open(fixture.bin), DiscImage::Status::Ok);
    EXPECT_EQ(image.representation(), DiscImage::Representation::RawSectors);
}

TEST_F(DiscImageTest, reads_an_iso_as_cooked_sectors) {
    EXPECT_EQ(image.open(fixture.iso), DiscImage::Status::Ok);
    EXPECT_EQ(image.representation(), DiscImage::Representation::CookedSectors);
}

TEST_F(DiscImageTest, reads_a_raw_image_as_raw_whatever_its_extension) {
    const std::filesystem::path renamed = fixture.directory / "raw-named.ISO";
    disk::copy_file(fixture.bin, renamed);
    EXPECT_EQ(image.open(renamed), DiscImage::Status::Ok);
    EXPECT_EQ(image.representation(), DiscImage::Representation::RawSectors);
}

TEST_F(DiscImageTest, rejects_bytes_that_are_not_a_disc_image) {
    const std::filesystem::path noise = fixture.directory / "noise.bin";
    SyntheticDisc::write(noise, SyntheticDisc::pattern(raw_sector_bytes * noise_sectors, noise_seed));
    EXPECT_EQ(image.open(noise), DiscImage::Status::UnsupportedFormat);
}

TEST_F(DiscImageTest, reports_a_missing_file) {
    EXPECT_EQ(image.open(fixture.directory / "absent.bin"), DiscImage::Status::NotFound);
}

}
