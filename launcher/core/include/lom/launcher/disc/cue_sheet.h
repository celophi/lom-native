#ifndef LOM_LAUNCHER_CUE_SHEET_H
#define LOM_LAUNCHER_CUE_SHEET_H

#include <filesystem>
#include <string>
#include <vector>

namespace lom::launcher {
class CueSheet {
  public:
    struct Track {
        int number = 0;
        std::string mode;
        std::size_t file_index = 0;
        long long index01_frames = -1;
    };

    enum class Status { Ok, Unreadable, Malformed };

    /**
     * @brief Parse a CUE file from disk.
     * @param path CUE file.
     * @return The parse status; on Ok, files() and tracks() describe the sheet.
     */
    Status load(const std::filesystem::path& path);

    /**
     * @brief Parse CUE text.
     * @param text CUE contents.
     * @param base_directory Directory used to resolve FILE names.
     * @return The parse status.
     */
    Status parse(const std::string& text, const std::filesystem::path& base_directory);

    /**
     * @brief Report the FILE entries resolved against the CUE's directory.
     * @return Paths in sheet order. A file whose exact name is absent but which
     *         differs only in letter case is resolved to the existing file.
     */
    const std::vector<std::filesystem::path>& files() const;

    /**
     * @brief Report the TRACK entries.
     * @return Tracks in sheet order.
     */
    const std::vector<Track>& tracks() const;

    /**
     * @brief Describe the first parse problem.
     * @return Text for diagnostics; empty on success.
     */
    const std::string& error() const;

  private:
    std::vector<std::filesystem::path> files_;
    std::vector<Track> tracks_;
    std::string error_;
};

}

#endif
