#ifndef LOM_LAUNCHER_MEMORY_CARD_H
#define LOM_LAUNCHER_MEMORY_CARD_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lom::launcher {
class MemoryCard {
  public:
    struct File {
        std::string name;
        int first_block = 0;
        int blocks = 0;
        std::vector<std::uint8_t> bytes;
        std::string problem;
    };

    enum class Status { Ok, Unreadable, WrongSize, UnsupportedWrapper, BadHeader };

    static constexpr std::size_t card_bytes = 128 * 1024;
    static constexpr std::size_t block_bytes = 8192;
    static constexpr std::size_t frame_bytes = 128;
    static constexpr int data_blocks = 15;

    /**
     * @brief Read and parse a card file.
     * @param path Card image.
     * @return Ok when the card header is valid; files() then lists its live files.
     */
    Status load(const std::filesystem::path& path);

    /**
     * @brief Parse card bytes.
     * @param bytes Whole card image.
     * @return Ok when the card header is valid.
     */
    Status parse(const std::vector<std::uint8_t>& bytes);

    /**
     * @brief Report the live files.
     * @return Files in directory order, including invalid ones with a problem.
     */
    const std::vector<File>& files() const;

    /**
     * @brief Describe the card-level problem or the wrapper format found.
     * @return Diagnostic text.
     */
    const std::string& error() const;

  private:
    std::vector<File> files_;
    std::string error_;
};

}

#endif
