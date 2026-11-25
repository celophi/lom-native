#ifndef LOM_LAUNCHER_SYNTHETIC_DISC_H
#define LOM_LAUNCHER_SYNTHETIC_DISC_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lom::launcher::test {
class SyntheticDisc {
  public:
    struct File {
        std::string path;
        std::vector<std::uint8_t> data;
        bool form2 = false;
    };

    /**
     * @brief Add a file.
     * @param path Canonical path.
     * @param data Contents.
     */
    void add(const std::string& path, std::vector<std::uint8_t> data);

    /**
     * @brief Add a file whose sectors are Mode 2 Form 2.
     * @param path Canonical path.
     * @param data Contents.
     */
    void add_form2(const std::string& path, std::vector<std::uint8_t> data);

    /**
     * @brief Build the disc as raw 2352-byte sectors.
     * @return Image bytes.
     */
    std::vector<std::uint8_t> raw() const;

    /**
     * @brief Build the disc as 2048-byte sectors.
     * @return Image bytes.
     */
    std::vector<std::uint8_t> cooked() const;

    /**
     * @brief Write bytes to a file.
     * @param path Destination.
     * @param bytes Contents.
     */
    static void write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes);

    /**
     * @brief Generate deterministic pseudo-random bytes.
     * @param size Number of bytes.
     * @param seed Seed.
     */
    static std::vector<std::uint8_t> pattern(std::size_t size, std::uint32_t seed);

  private:
    /**
     * @brief Lay out the user data of every sector.
     * @param form2 Receives which sectors are Form 2.
     * @return 2048-byte user-data blocks, one per sector.
     */
    std::vector<std::vector<std::uint8_t>> sectors(std::vector<bool>& form2) const;

    std::vector<File> files_;
};

}

#endif
