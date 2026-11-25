#ifndef LOM_LAUNCHER_ISO9660_READER_H
#define LOM_LAUNCHER_ISO9660_READER_H

#include <cstdint>
#include <string>
#include <vector>

namespace lom::launcher {

class DiscImage;

class Iso9660Reader {
  public:
    struct File {
        std::string path;
        std::uint32_t lba = 0;
        std::uint32_t bytes = 0;
    };

    static constexpr int max_depth = 16;
    static constexpr std::size_t max_files = 20000;

    /**
     * @brief Read the volume descriptor and every directory.
     * @param image Opened image.
     * @return True when the whole tree is valid; files() then lists it in disc order.
     */
    bool read(DiscImage& image);

    /**
     * @brief Read the system identifier and the boot executable named by SYSTEM.CNF.
     * @param image Opened image; read() must have succeeded.
     * @return Executable file name such as "SLUS_010.13", or empty when absent.
     */
    std::string boot_executable(DiscImage& image) const;

    /**
     * @brief Read one file's bytes.
     * @param image Opened image.
     * @param file Entry from files().
     * @param out Receives the contents.
     * @return False when a sector is unreadable or not Form 1 data.
     */
    static bool read_file(DiscImage& image, const File& file, std::vector<std::uint8_t>& out);

    /**
     * @brief Report the files found by read().
     * @return Regular files.
     */
    const std::vector<File>& files() const;

    /**
     * @brief Describe the first problem.
     * @return Diagnostic text.
     */
    const std::string& error() const;

  private:
    /**
     * @brief Walk one directory extent.
     * @param image Opened image.
     * @param lba Directory extent start.
     * @param bytes Directory extent size.
     * @param prefix Canonical path prefix ending in '/' or empty.
     * @param depth Current depth.
     * @return False on any invalid record.
     */
    bool walk(DiscImage& image, std::uint32_t lba, std::uint32_t bytes, const std::string& prefix, int depth);

    std::vector<File> files_;
    std::vector<std::uint32_t> visited_directories_;
    std::string error_;
};

}

#endif
