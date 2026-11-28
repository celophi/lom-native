#include "lom/launcher/disc/cue_sheet.h"
#include "lom/launcher/common/files_provider.h"

#include "lom/launcher/common/atomic_file.h"
#include "common/text_case.h"

#include <sstream>
#include <system_error>

namespace lom::launcher {

namespace {
constexpr std::size_t max_cue_bytes = 64 * 1024;
constexpr int frames_per_second = 75;
constexpr int seconds_per_minute = 60;

/**
 * @brief Read the next token, honoring double-quoted strings.
 * @param line Source line.
 * @param position Cursor; advanced past the token.
 * @param token Receives the token without quotes.
 * @return False when no token remains or a quote is unterminated.
 */
bool next_token(const std::string& line, std::size_t& position, std::string& token) {
    while (position < line.size() && (line[position] == ' ' || line[position] == '\t')) {
        position++;
    }

    if (position >= line.size()) {
        return false;
    }

    token.clear();

    if (line[position] == '"') {
        const std::size_t end = line.find('"', position + 1);

        if (end == std::string::npos) {
            return false;
        }

        token = line.substr(position + 1, end - position - 1);
        position = end + 1;
        return true;
    }

    while (position < line.size() && line[position] != ' ' && line[position] != '\t') {
        token.push_back(line[position++]);
    }

    return true;
}

/**
 * @brief Parse an MM:SS:FF position.
 * @param text Position text.
 * @return Frames, or -1 when malformed.
 */
long long parse_msf(const std::string& text) {
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
    char colon1 = 0;
    char colon2 = 0;
    std::istringstream in(text);

    if (!(in >> minutes >> colon1 >> seconds >> colon2 >> frames) || colon1 != ':' || colon2 != ':' || seconds >= seconds_per_minute ||
        frames >= frames_per_second || minutes < 0 || seconds < 0 || frames < 0) {
        return -1;
    }

    return (static_cast<long long>(minutes) * seconds_per_minute + seconds) * frames_per_second + frames;
}

/**
 * @brief Resolve a FILE name, accepting a single case-insensitive match.
 * @param base Directory of the CUE file.
 * @param name Name from the sheet.
 * @return The existing path when found, otherwise base / name.
 */
std::filesystem::path resolve_track_file(const std::filesystem::path& base, const std::string& name) {
    const std::filesystem::path direct = base / std::filesystem::u8path(name);
    std::error_code code;

    if (files_provider().exists(direct, code)) {
        return direct;
    }

    const std::string wanted = ascii_upper(std::filesystem::u8path(name).filename().u8string());
    std::filesystem::path found;
    int matches = 0;

    for (const FileEntry& entry : files_provider().list(direct.parent_path(), false, code)) {
        if (ascii_upper(entry.path.filename().u8string()) == wanted) {
            found = entry.path;
            matches++;
        }
    }

    return matches == 1 ? found : direct;
}

}

CueSheet::Status CueSheet::load(const std::filesystem::path& path) {
    std::error_code code;
    const std::uintmax_t size = files_provider().file_size(path, code);

    if (code) {
        error_ = "cannot read the CUE file";
        return Status::Unreadable;
    }

    if (size > max_cue_bytes) {
        error_ = "file is too large to be a CUE descriptor";
        return Status::Malformed;
    }

    std::string read_error;
    const auto text = AtomicFile::read(path, &read_error);

    if (!text) {
        error_ = read_error;
        return Status::Unreadable;
    }

    return parse(*text, path.parent_path());
}

CueSheet::Status CueSheet::parse(const std::string& text, const std::filesystem::path& base_directory) {
    files_.clear();
    tracks_.clear();
    error_.clear();

    std::istringstream lines(text);
    std::string line;

    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        for (const char c : line) {
            if (static_cast<unsigned char>(c) < 0x20 && c != '\t') {
                error_ = "binary data in CUE text";
                return Status::Malformed;
            }
        }

        std::size_t position = 0;
        std::string command;

        if (!next_token(line, position, command)) {
            continue;
        }

        command = ascii_upper(command);

        if (command == "FILE") {
            std::string name;

            if (!next_token(line, position, name) || name.empty()) {
                error_ = "FILE without a name";
                return Status::Malformed;
            }

            files_.push_back(resolve_track_file(base_directory, name));
        } else if (command == "TRACK") {
            std::string number;
            std::string mode;

            if (files_.empty() || !next_token(line, position, number) || !next_token(line, position, mode)) {
                error_ = "TRACK before FILE or without a mode";
                return Status::Malformed;
            }

            Track track;
            track.number = std::atoi(number.c_str());
            track.mode = ascii_upper(mode);
            track.file_index = files_.size() - 1;
            tracks_.push_back(track);
        } else if (command == "INDEX") {
            std::string number;
            std::string position_text;

            if (tracks_.empty() || !next_token(line, position, number) || !next_token(line, position, position_text)) {
                error_ = "INDEX outside a track";
                return Status::Malformed;
            }

            const long long frames = parse_msf(position_text);

            if (frames < 0) {
                error_ = "invalid INDEX position";
                return Status::Malformed;
            }

            if (std::atoi(number.c_str()) == 1) {
                tracks_.back().index01_frames = frames;
            }
        }
    }

    if (files_.empty() || tracks_.empty()) {
        error_ = "no FILE or TRACK entries";
        return Status::Malformed;
    }

    return Status::Ok;
}

/**
 * @brief Report the FILE entries resolved against the CUE's directory.
 * @return Paths in sheet order.
 */
const std::vector<std::filesystem::path>& CueSheet::files() const {
    return files_;
}

const std::vector<CueSheet::Track>& CueSheet::tracks() const {
    return tracks_;
}

const std::string& CueSheet::error() const {
    return error_;
}

}
