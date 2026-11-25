#include "../support/synthetic_card.h"
#include "lom/launcher/saves/lom_save.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using namespace lom::launcher;
using namespace lom::launcher::test;

constexpr std::size_t checksummed_byte = 100;

constexpr std::size_t magic_offset = 0x33E4;

constexpr std::uint32_t save_seed = 1;

class LomSaveTest : public ::testing::Test {
  protected:
    std::vector<std::uint8_t> save = SyntheticCard::save(save_seed);
};

TEST_F(LomSaveTest, accepts_a_save_as_the_game_writes_it) {
    EXPECT_EQ(LomSave::validate(save.data(), save.size()), LomSave::Status::Valid);
}

TEST_F(LomSaveTest, rejects_a_save_whose_checksummed_bytes_changed) {
    save[checksummed_byte] ^= 1;
    EXPECT_EQ(LomSave::validate(save.data(), save.size()), LomSave::Status::BadChecksum);
}

TEST_F(LomSaveTest, rejects_a_save_with_the_wrong_magic_number) {
    save[magic_offset] ^= 1;
    EXPECT_EQ(LomSave::validate(save.data(), save.size()), LomSave::Status::BadMagic);
}

TEST_F(LomSaveTest, rejects_a_save_without_the_memory_card_header) {
    save[0] = 'X';
    EXPECT_EQ(LomSave::validate(save.data(), save.size()), LomSave::Status::BadHeader);
}

TEST_F(LomSaveTest, rejects_a_save_of_the_wrong_size) {
    EXPECT_EQ(LomSave::validate(save.data(), save.size() - 1), LomSave::Status::WrongSize);
}

}
