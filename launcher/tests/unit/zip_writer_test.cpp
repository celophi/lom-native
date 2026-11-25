#include "lom/launcher/common/zip_writer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace {

using namespace lom::launcher;

/** 2026-11-19 12:00:00 local time. */
std::time_t sample_time() {
    std::tm local{};
    local.tm_year = 2026 - 1900;
    local.tm_mon = 10;
    local.tm_mday = 19;
    local.tm_hour = 12;
    local.tm_isdst = -1;
    return std::mktime(&local);
}

/**
 * @brief Read a little-endian 16-bit number from a zip.
 * @param bytes The zip.
 * @param at Where the number is.
 * @return The number.
 */
std::uint16_t read16(const std::string& bytes, std::size_t at) {
    return std::uint16_t(std::uint8_t(bytes[at]) | (std::uint8_t(bytes[at + 1]) << 8));
}

/**
 * @brief Read a little-endian 32-bit number from a zip.
 * @param bytes The zip.
 * @param at Where the number is.
 * @return The number.
 */
std::uint32_t read32(const std::string& bytes, std::size_t at) {
    return std::uint32_t(read16(bytes, at)) | (std::uint32_t(read16(bytes, at + 2)) << 16);
}

TEST(ZipWriter, uses_the_standard_crc32) {
    EXPECT_EQ(ZipWriter::crc32("123456789"), 0xCBF43926u);
    EXPECT_EQ(ZipWriter::crc32(""), 0u);
}

TEST(ZipWriter, stores_each_file_with_a_local_header_and_a_directory_entry) {
    ZipWriter zip;
    zip.add("report.txt", "hello", sample_time());
    zip.add("game/game-1.log", "log", sample_time());
    const std::string archive = zip.finish();

    EXPECT_EQ(read32(archive, 0), 0x04034b50u);
    EXPECT_EQ(read16(archive, 8), 0);
    EXPECT_EQ(read32(archive, 14), ZipWriter::crc32("hello"));
    EXPECT_EQ(read32(archive, 18), 5u);
    EXPECT_EQ(archive.substr(30, 10), "report.txt");
    EXPECT_EQ(archive.substr(40, 5), "hello");

    const std::size_t end = archive.size() - 22;
    EXPECT_EQ(read32(archive, end), 0x06054b50u);
    EXPECT_EQ(read16(archive, end + 10), 2);
    const std::uint32_t directory = read32(archive, end + 16);
    EXPECT_EQ(read32(archive, directory), 0x02014b50u);
    EXPECT_EQ(archive.substr(directory + 46, 10), "report.txt");
}

TEST(ZipWriter, deflates_only_when_the_compressor_makes_it_smaller) {
    ZipWriter zip([](const std::string& data) -> std::optional<std::string> {
        return data == "aaaaaaaa" ? std::optional<std::string>("xy") : std::optional<std::string>(data + "longer");
    });
    zip.add("small", "aaaaaaaa", sample_time());
    zip.add("big", "abc", sample_time());
    const std::string archive = zip.finish();

    EXPECT_EQ(read16(archive, 8), 8);
    EXPECT_EQ(read32(archive, 14), ZipWriter::crc32("aaaaaaaa"));
    EXPECT_EQ(read32(archive, 18), 2u);
    EXPECT_EQ(read32(archive, 22), 8u);

    const std::size_t second = 30 + 5 + 2;
    EXPECT_EQ(read16(archive, second + 8), 0);
    EXPECT_EQ(archive.substr(second + 30 + 3, 3), "abc");
}

}
