#include "lom/launcher/common/atomic_file.h"
#include "lom/launcher/common/files_provider.h"

#include "lom/launcher/common/random_provider.h"

#include <istream>
#include <ostream>
#include <system_error>

namespace lom::launcher {

namespace {

constexpr char temporary_marker[] = ".tmp-";

/**
 * @brief Build a unique sibling path for a temporary write.
 * @param path Final destination.
 * @return Temporary path in the same directory.
 */
std::filesystem::path temporary_sibling(const std::filesystem::path& path) {
    std::filesystem::path temporary = path;
    temporary += temporary_marker + std::to_string(RandomProvider::system().next()) + "-" + std::to_string(RandomProvider::system().next());
    return temporary;
}

}

bool AtomicFile::write(const std::filesystem::path& path, const std::string& contents, std::string* error) {
    std::error_code code;

    if (path.has_parent_path()) {
        files_provider().create_directories(path.parent_path(), code);

        if (code) {
            if (error != nullptr) {
                *error = "cannot create " + path.parent_path().u8string() + ": " + code.message();
            }

            return false;
        }
    }

    const std::filesystem::path temporary = temporary_sibling(path);

    bool written = false;

    {
        const auto out_file = files_provider().open_write(temporary, std::ios::binary | std::ios::trunc);
        std::ostream& out = *out_file;
        out.write(contents.data(), std::streamsize(contents.size()));
        out.flush();
        written = static_cast<bool>(out);
    }

    if (!written) {
        files_provider().remove(temporary, code);

        if (error != nullptr) {
            *error = "cannot write " + temporary.u8string();
        }

        return false;
    }

    files_provider().rename(temporary, path, code);

    if (code) {
        std::error_code ignored;
        files_provider().remove(temporary, ignored);

        if (error != nullptr) {
            *error = "cannot replace " + path.u8string() + ": " + code.message();
        }

        return false;
    }

    return true;
}

std::optional<std::string> AtomicFile::read(const std::filesystem::path& path, std::string* error) {
    std::error_code code;
    const std::uintmax_t size = files_provider().file_size(path, code);

    if (code) {
        if (error != nullptr) {
            *error = "cannot read " + path.u8string() + ": " + code.message();
        }

        return std::nullopt;
    }

    if (size > max_read_bytes) {
        if (error != nullptr) {
            *error = path.u8string() + " is too large";
        }

        return std::nullopt;
    }

    const auto in_file = files_provider().open_read(path, std::ios::binary);
    std::istream& in = *in_file;
    std::string contents(std::size_t(size), '\0');
    in.read(contents.data(), std::streamsize(size));

    if (!in || in.gcount() != std::streamsize(size)) {
        if (error != nullptr) {
            *error = "cannot read " + path.u8string();
        }

        return std::nullopt;
    }

    return contents;
}

bool AtomicFile::read_bytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes) {
    const std::optional<std::string> contents = read(path, nullptr);

    if (!contents) {
        return false;
    }

    bytes.assign(contents->begin(), contents->end());
    return true;
}

}
