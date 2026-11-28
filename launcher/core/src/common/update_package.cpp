#include "lom/launcher/common/update_package.h"
#include "lom/launcher/common/files_provider.h"

#include <cctype>
#include <sstream>
#include <algorithm>
#include <system_error>
#include <vector>

namespace lom::launcher {

namespace {

constexpr char asset_prefix[] = "lom-native-";
constexpr char appimage_suffix[] = "-linux-x86_64.AppImage";
constexpr char windows_suffix[] = "-windows-x64.zip";
constexpr char mac_suffix[] = "-macos-arm64.dmg";
constexpr std::size_t digest_length = 64;

/**
 * @brief The path an update keeps an old file or folder at.
 * @param path The file or folder.
 * @return It with UpdatePackage::leftover_suffix added.
 */
std::filesystem::path leftover(const std::filesystem::path& path) {
    std::filesystem::path old = path;
    old += UpdatePackage::leftover_suffix;
    return old;
}

/**
 * @brief Check that text is a SHA-256 in hex.
 * @param text The text.
 * @return True when it's 64 hex digits.
 */
bool is_digest(const std::string& text) {
    if (text.size() != digest_length) {
        return false;
    }

    for (const char c : text) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Describe a failed step.
 * @param what What was being done.
 * @param path The path it was done to.
 * @param code Why it failed.
 * @return Like "couldn't rename /a/b: Permission denied".
 */
std::string failure(const char* what, const std::filesystem::path& path, const std::error_code& code) {
    return std::string("couldn't ") + what + " " + path.u8string() + ": " + code.message();
}

/**
 * @brief Swap a file or folder for a new one, keeping the old one beside it.
 * @param target What to replace.
 * @param replacement The new one.
 * @param error Set to why it failed.
 * @return True when the new one is in place; on failure the old one is back.
 */
bool swap_in(const std::filesystem::path& target, const std::filesystem::path& replacement, std::string* error) {
    std::error_code code;
    const std::filesystem::path old = leftover(target);
    files_provider().remove_all(old, code);
    files_provider().rename(target, old, code);

    if (code) {
        *error = failure("move aside", target, code);
        return false;
    }

    files_provider().rename(replacement, target, code);

    if (code) {
        *error = failure("put in place", target, code);
        std::error_code ignored;
        files_provider().rename(old, target, ignored);
        return false;
    }

    return true;
}

}

std::string UpdatePackage::asset_name(Kind kind, const std::string& version) {
    switch (kind) {
    case Kind::AppImage:
        return asset_prefix + version + appimage_suffix;
    case Kind::WindowsFolder:
        return asset_prefix + version + windows_suffix;
    case Kind::MacBundle:
        return asset_prefix + version + mac_suffix;
    }

    return std::string();
}

std::optional<std::string> UpdatePackage::checksum(const std::string& sums, const std::string& file_name) {
    std::istringstream lines(sums);
    std::string line;

    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::size_t space = line.find(' ');

        if (space == std::string::npos || space + 1 >= line.size()) {
            continue;
        }

        std::string digest = line.substr(0, space);
        std::string name = line.substr(space + 1);

        if (!name.empty() && (name.front() == ' ' || name.front() == '*')) {
            name.erase(0, 1);
        }

        if (name == file_name && is_digest(digest)) {
            for (char& c : digest) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            return digest;
        }
    }

    return std::nullopt;
}

bool UpdatePackage::replace_file(const std::filesystem::path& target, const std::filesystem::path& replacement, std::string* error) {
    if (!swap_in(target, replacement, error)) {
        return false;
    }

    std::error_code ignored;
    files_provider().remove(leftover(target), ignored);
    return true;
}

bool UpdatePackage::replace_folder(const std::filesystem::path& target, const std::filesystem::path& replacement, std::string* error) {
    struct Placed {
        std::filesystem::path source;
        std::filesystem::path destination;
        bool kept_old = false;
    };

    std::error_code code;
    std::vector<std::filesystem::path> files;

    for (const FileEntry& entry : files_provider().list(replacement, true, code)) {
        if (entry.regular) {
            files.push_back(entry.path);
        }
    }

    if (code) {
        *error = failure("read", replacement, code);
        return false;
    }

    std::vector<Placed> placed;
    const auto undo = [&placed]() {
        for (auto it = placed.rbegin(); it != placed.rend(); ++it) {
            std::error_code ignored;
            files_provider().rename(it->destination, it->source, ignored);

            if (it->kept_old) {
                files_provider().rename(leftover(it->destination), it->destination, ignored);
            }
        }
    };

    for (const std::filesystem::path& source : files) {
        Placed step{source, target / std::filesystem::relative(source, replacement)};
        files_provider().create_directories(step.destination.parent_path(), code);

        if (code) {
            *error = failure("make", step.destination.parent_path(), code);
            undo();
            return false;
        }

        if (files_provider().exists(step.destination, code)) {
            std::error_code ignored;
            files_provider().remove(leftover(step.destination), ignored);
            files_provider().rename(step.destination, leftover(step.destination), code);

            if (code) {
                *error = failure("move aside", step.destination, code);
                undo();
                return false;
            }

            step.kept_old = true;
        }

        files_provider().rename(step.source, step.destination, code);

        if (code) {
            *error = failure("put in place", step.destination, code);

            if (step.kept_old) {
                std::error_code ignored;
                files_provider().rename(leftover(step.destination), step.destination, ignored);
            }

            undo();
            return false;
        }

        placed.push_back(step);
    }

    return true;
}

bool UpdatePackage::replace_bundle(const std::filesystem::path& target, const std::filesystem::path& replacement, std::string* error) {
    return swap_in(target, replacement, error);
}

void UpdatePackage::remove_leftovers(const std::filesystem::path& target) {
    std::error_code code;
    files_provider().remove_all(leftover(target), code);

    if (!files_provider().is_directory(target, code)) {
        return;
    }

    std::vector<std::filesystem::path> old;
    const std::string suffix = leftover_suffix;

    for (const FileEntry& entry : files_provider().list(target, true, code)) {
        const std::string name = entry.path.filename().u8string();

        if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            old.push_back(entry.path);
        }
    }

    std::sort(old.begin(), old.end());
    std::vector<std::filesystem::path> outermost;

    for (const std::filesystem::path& path : old) {
        const std::string inside = outermost.empty() ? std::string() : outermost.back().generic_u8string() + "/";

        if (outermost.empty() || path.generic_u8string().compare(0, inside.size(), inside) != 0) {
            outermost.push_back(path);
        }
    }

    for (const std::filesystem::path& path : outermost) {
        files_provider().remove_all(path, code);
    }
}

}
