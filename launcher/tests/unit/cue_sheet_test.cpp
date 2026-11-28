#include "../support/disc_fixture.h"
#include "../support/disk.h"
#include "../support/test_harness.h"
#include "lom/launcher/disc/disc_image.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using namespace lom::launcher;
using namespace lom::launcher::test;

struct Sheet {
    const char* name;
    std::string text;
    DiscImage::Status expected;
};

class CueSheetParsing : public ::testing::TestWithParam<Sheet> {};

TEST_P(CueSheetParsing, opens_or_rejects_the_sheet) {
    const DiscFixture fixture = DiscFixture::create(TestHarness::instance().scratch_directory());
    const std::filesystem::path sheet = fixture.cue.parent_path() / "sheet.cue";
    *disk::write(sheet, std::ios::binary) << GetParam().text;
    DiscImage image;
    EXPECT_EQ(image.open(sheet), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(
    CueSheet, CueSheetParsing,
    ::testing::Values(
        Sheet{"finds_the_bin_when_its_name_differs_only_in_case", "FILE \"test disc (usa).BIN\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\n",
              DiscImage::Status::Ok},
        Sheet{"reports_a_bin_that_is_not_there", "FILE \"gone.bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\n", DiscImage::Status::MissingTrackFile},
        Sheet{"rejects_a_second_track", "FILE \"Test Disc (USA).bin\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 01 10:00:00\n",
              DiscImage::Status::UnsupportedLayout},
        Sheet{"rejects_an_audio_track", "FILE \"Test Disc (USA).bin\" BINARY\nTRACK 01 AUDIO\nINDEX 01 00:00:00\n", DiscImage::Status::UnsupportedLayout},
        Sheet{"rejects_binary_garbage", std::string("FILE\0\x01\x02", 7), DiscImage::Status::Malformed},
        Sheet{"rejects_a_cooked_image_described_as_raw_sectors", "FILE \"Test Disc.iso\" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\n",
              DiscImage::Status::Malformed}),
    [](const ::testing::TestParamInfo<Sheet>& info) {
        return std::string(info.param.name);
    });

}
