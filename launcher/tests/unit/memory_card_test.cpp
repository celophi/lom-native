#include "../support/synthetic_card.h"
#include "../support/synthetic_disc.h"
#include "../support/test_harness.h"
#include "lom/launcher/saves/memory_card.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using namespace lom::launcher;
using namespace lom::launcher::test;

constexpr char lom_save[] = "BASLUS-010130-1";
constexpr char second_lom_save[] = "BASLUS-010131-2";
constexpr char other_game[] = "BASLUS-00001DATA";
constexpr char deleted_file[] = "BASLUS-01013DEL";

constexpr std::uint8_t deleted_first_block = 0xA1;

constexpr std::size_t next_block_offset = 8;

constexpr std::size_t covered_frame_byte = 20;

constexpr std::uint8_t block_outside_the_card = 20;

constexpr std::size_t dexdrive_header_bytes = 3904;

constexpr std::size_t short_file_bytes = 1000;

constexpr std::uint32_t save_seed = 4;
constexpr std::uint32_t other_game_seed = 5;
constexpr std::uint32_t deleted_seed = 6;

/**
 * @brief Directory frame index of a data block; frame 0 is the card header.
 * @param block Block number from 0 to 14.
 * @return Frame index for SyntheticCard::seal().
 */
constexpr int frame_of(int block) {
    return block + 1;
}

/**
 * @brief Find a parsed file by name.
 * @param card Parsed card.
 * @param name File name.
 * @return The file, or null.
 */
const MemoryCard::File* find(const MemoryCard& card, const std::string& name) {
    for (const MemoryCard::File& file : card.files()) {
        if (file.name == name) {
            return &file;
        }
    }

    return nullptr;
}

TEST(MemoryCard, rebuilds_a_save_stored_in_blocks_out_of_order) {
    const std::vector<std::uint8_t> save = SyntheticCard::save(save_seed);
    SyntheticCard builder;
    builder.add(lom_save, {5, 2}, save);
    MemoryCard card;
    ASSERT_EQ(card.parse(builder.bytes()), MemoryCard::Status::Ok);

    const MemoryCard::File* file = find(card, lom_save);
    ASSERT_NE(file, nullptr);
    EXPECT_TRUE(file->problem.empty());
    EXPECT_EQ(file->blocks, 2);
    EXPECT_EQ(file->bytes, save);
}

TEST(MemoryCard, lists_other_games_saves_but_not_deleted_files) {
    SyntheticCard builder;
    builder.add(lom_save, {5, 2}, SyntheticCard::save(save_seed));
    builder.add(other_game, {0}, SyntheticDisc::pattern(MemoryCard::block_bytes, other_game_seed));
    builder.add(deleted_file, {9}, SyntheticDisc::pattern(MemoryCard::block_bytes, deleted_seed));
    builder.frame(9)[0] = deleted_first_block;
    builder.seal(frame_of(9));
    MemoryCard card;
    ASSERT_EQ(card.parse(builder.bytes()), MemoryCard::Status::Ok);

    EXPECT_NE(find(card, lom_save), nullptr);
    EXPECT_NE(find(card, other_game), nullptr);
    EXPECT_EQ(find(card, deleted_file), nullptr);
}

TEST(MemoryCard, reports_a_block_chain_that_loops) {
    SyntheticCard builder;
    builder.add(lom_save, {3, 4}, SyntheticCard::save(save_seed));
    builder.frame(4)[next_block_offset] = 3;
    builder.seal(frame_of(4));
    MemoryCard card;
    ASSERT_EQ(card.parse(builder.bytes()), MemoryCard::Status::Ok);

    const MemoryCard::File* file = find(card, lom_save);
    ASSERT_NE(file, nullptr);
    EXPECT_FALSE(file->problem.empty());
}

TEST(MemoryCard, reports_files_that_claim_the_same_block) {
    const std::vector<std::uint8_t> save = SyntheticCard::save(save_seed);
    SyntheticCard builder;
    builder.add(lom_save, {1, 2}, save);
    builder.add(second_lom_save, {6, 2}, save);
    MemoryCard card;
    ASSERT_EQ(card.parse(builder.bytes()), MemoryCard::Status::Ok);

    ASSERT_EQ(card.files().size(), 2u);

    for (const MemoryCard::File& file : card.files()) {
        EXPECT_FALSE(file.problem.empty()) << file.name;
    }
}

TEST(MemoryCard, reports_a_directory_frame_with_a_bad_checksum) {
    SyntheticCard builder;
    builder.add(lom_save, {7, 8}, SyntheticCard::save(save_seed));
    builder.frame(8)[covered_frame_byte] ^= 1;
    MemoryCard card;
    ASSERT_EQ(card.parse(builder.bytes()), MemoryCard::Status::Ok);

    ASSERT_EQ(card.files().size(), 1u);
    EXPECT_FALSE(card.files()[0].problem.empty());
}

TEST(MemoryCard, reports_a_link_to_a_block_outside_the_card) {
    SyntheticCard builder;
    builder.add(lom_save, {0, 1}, SyntheticCard::save(save_seed));
    builder.frame(0)[next_block_offset] = block_outside_the_card;
    builder.seal(frame_of(0));
    MemoryCard card;
    ASSERT_EQ(card.parse(builder.bytes()), MemoryCard::Status::Ok);

    ASSERT_FALSE(card.files().empty());
    EXPECT_FALSE(card.files()[0].problem.empty());
}

TEST(MemoryCard, recognises_a_dexdrive_file_as_unsupported) {
    std::vector<std::uint8_t> dexdrive(MemoryCard::card_bytes + dexdrive_header_bytes, 0);
    std::copy_n("123-456-STD", 11, dexdrive.begin());
    MemoryCard card;
    EXPECT_EQ(card.parse(dexdrive), MemoryCard::Status::UnsupportedWrapper);
}

TEST(MemoryCard, rejects_a_file_of_the_wrong_size) {
    MemoryCard card;
    EXPECT_EQ(card.parse(std::vector<std::uint8_t>(short_file_bytes, 0)), MemoryCard::Status::WrongSize);
}

TEST(MemoryCard, rejects_a_card_without_its_header) {
    MemoryCard card;
    EXPECT_EQ(card.parse(std::vector<std::uint8_t>(MemoryCard::card_bytes, 0)), MemoryCard::Status::BadHeader);
}

class MemoryCardExtension : public ::testing::TestWithParam<const char*> {};

TEST_P(MemoryCardExtension, loads_a_raw_card_whatever_its_extension) {
    const std::filesystem::path path = TestHarness::instance().scratch_directory() / GetParam();
    SyntheticCard builder;
    builder.add(lom_save, {0, 1}, SyntheticCard::save(save_seed));
    SyntheticDisc::write(path, builder.bytes());
    MemoryCard card;
    EXPECT_EQ(card.load(path), MemoryCard::Status::Ok);
    EXPECT_EQ(card.files().size(), 1u);
}

INSTANTIATE_TEST_SUITE_P(MemoryCard, MemoryCardExtension, ::testing::Values("card.mcr", "card.MCD", "card.bin"),
                         [](const ::testing::TestParamInfo<const char*>& info) {
                             std::string name = info.param;
                             std::replace(name.begin(), name.end(), '.', '_');
                             return name;
                         });

}
