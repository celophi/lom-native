#ifndef LOM_LAUNCHER_DISC_IMAGE_H
#define LOM_LAUNCHER_DISC_IMAGE_H

#include "lom/launcher/common/progress.h"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <memory>
#include <sstream>
#include <string>

namespace lom::launcher {
class DiscImage {
  public:
    enum class Representation { Unknown, RawSectors, CookedSectors };

    enum class Status { Ok, NotFound, Unreadable, Malformed, MissingTrackFile, UnsupportedLayout, UnsupportedFormat };

    enum class SectorKind { Mode1, Mode2Form1, Mode2Form2, Cooked };

    static constexpr std::uint32_t raw_sector_bytes = 2352;
    static constexpr std::uint32_t user_sector_bytes = 2048;
    static constexpr char raw_single_track_layout[] = "single-track-raw-2352";
    static constexpr char cooked_layout[] = "cooked-2048";

    /**
     * @brief Open and classify an image.
     * @param selected The file the user chose (.cue, .bin or .iso, any case).
     * @return Ok when the image has a recognized layout and readable data.
     */
    Status open(const std::filesystem::path& selected);

    /**
     * @brief Report how sectors are stored.
     * @return The representation; Unknown before a successful open().
     */
    Representation representation() const;

    /**
     * @brief Report the data file's size.
     * @return Bytes in the data file.
     */
    std::uint64_t data_bytes() const;

    /**
     * @brief Report the number of whole sectors.
     * @return Sector count.
     */
    std::uint32_t sector_count() const;

    /**
     * @brief Report the recognized layout name used by the release catalog.
     * @return raw_single_track_layout or cooked_layout.
     */
    const char* layout() const;

    /**
     * @brief Hash the complete data file with SHA-1.
     * @param progress Receives CheckingImage progress in bytes; may request cancellation.
     * @param sha1 Receives the lowercase digest.
     * @return False when cancelled, unreadable or changed during hashing.
     */
    bool hash_identity(const Progress& progress, std::string& sha1);

    /**
     * @brief Read the 2048 user-data bytes of a sector.
     * @param lba Logical sector number.
     * @param out Receives user_sector_bytes bytes.
     * @param kind Receives the sector form; may be null.
     * @return False when the sector is out of range, unreadable or not a data sector.
     */
    bool read_user_data(std::uint32_t lba, std::uint8_t* out, SectorKind* kind);

    /**
     * @brief Check that the data file still has the size and modification time seen when opened.
     * @return True when unchanged.
     */
    bool unchanged_since_open() const;

    /**
     * @brief Describe the last problem.
     * @return Diagnostic text.
     */
    const std::string& error() const;

  private:
    /**
     * @brief Open the data file and classify its sectors.
     * @param path Data file.
     * @param require_raw Whether only raw sectors are acceptable (CUE tracks).
     * @return Open status.
     */
    Status open_data(const std::filesystem::path& path, bool require_raw);

    std::filesystem::path data_file_;
    std::unique_ptr<std::istream> stream_ = std::make_unique<std::istringstream>();
    Representation representation_ = Representation::Unknown;
    std::uint64_t data_bytes_ = 0;
    std::filesystem::file_time_type modified_{};
    std::string error_;
};

}

#endif
