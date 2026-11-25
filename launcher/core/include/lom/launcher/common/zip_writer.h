#ifndef LOM_LAUNCHER_ZIP_WRITER_H
#define LOM_LAUNCHER_ZIP_WRITER_H

#include <cstdint>
#include <ctime>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace lom::launcher {
class ZipWriter {
  public:
    using Compressor = std::function<std::optional<std::string>(const std::string& data)>;

    /**
     * @brief Start an empty archive.
     * @param compressor Deflate for the entries; empty stores everything.
     */
    explicit ZipWriter(Compressor compressor = {});

    /**
     * @brief Add a file.
     * @param name Path inside the archive, with "/" between folders.
     * @param data File contents.
     * @param modified When the file was last changed, in local time as zip expects.
     */
    void add(const std::string& name, const std::string& data, std::time_t modified);

    /**
     * @brief Finish the archive.
     * @return The complete zip file.
     */
    std::string finish() const;

    /**
     * @brief CRC-32 as zip uses it (the IEEE 802.3 polynomial).
     * @param data Bytes.
     * @return Checksum.
     */
    static std::uint32_t crc32(const std::string& data);

  private:
    struct Entry {
        std::string name;
        std::uint32_t crc = 0;
        std::uint32_t compressed_size = 0;
        std::uint32_t size = 0;
        std::uint16_t method = 0;
        std::uint16_t dos_time = 0;
        std::uint16_t dos_date = 0;
        std::uint32_t offset = 0;
    };

    Compressor compressor_;
    std::string body_;
    std::vector<Entry> entries_;
};

}

#endif
