#include "../support/disc_fixture.h"
#include "../support/synthetic_disc.h"
#include "../support/test_harness.h"
#include "lom/launcher/disc/disc_image.h"
#include "lom/launcher/disc/iso9660_reader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

namespace {

using namespace lom::launcher;
using namespace lom::launcher::test;

constexpr std::size_t standard_file_count = 5;

constexpr std::size_t form2_bytes = 4096;
constexpr std::uint32_t form2_seed = 5;

class Iso9660ReaderTest : public ::testing::Test {
  protected:
    /**
     * @brief Open the raw image.
     */
    Iso9660ReaderTest() {
        EXPECT_EQ(image.open(fixture.bin), DiscImage::Status::Ok);
    }

    DiscFixture fixture = DiscFixture::create(TestHarness::instance().scratch_directory());
    DiscImage image;
    Iso9660Reader reader;
};

TEST_F(Iso9660ReaderTest, lists_every_file_on_the_disc) {
    ASSERT_TRUE(reader.read(image));
    EXPECT_EQ(reader.files().size(), standard_file_count);
}

TEST_F(Iso9660ReaderTest, finds_the_boot_executable_named_in_system_cnf) {
    ASSERT_TRUE(reader.read(image));
    EXPECT_EQ(reader.boot_executable(image), DiscFixture::executable);
}

TEST(Iso9660Reader, refuses_to_read_form2_sectors_as_file_data) {
    const std::filesystem::path root = TestHarness::instance().scratch_directory();
    SyntheticDisc disc;
    disc.add("SYSTEM.CNF", {'B'});
    disc.add_form2("XA.STR", SyntheticDisc::pattern(form2_bytes, form2_seed));
    SyntheticDisc::write(root / "form2.bin", disc.raw());

    DiscImage image;
    Iso9660Reader reader;
    ASSERT_EQ(image.open(root / "form2.bin"), DiscImage::Status::Ok);
    ASSERT_TRUE(reader.read(image));
    ASSERT_EQ(reader.files().size(), 2u);
    std::vector<std::uint8_t> bytes;
    EXPECT_FALSE(Iso9660Reader::read_file(image, reader.files()[1], bytes));
}

}
