#include "disc/file_digest.h"
#include "lom/launcher/common/files_provider.h"

#include "lom/launcher/common/sha256.h"

#include <istream>
#include <ostream>
#include <vector>

namespace lom::launcher {

namespace {

constexpr std::size_t chunk_bytes = 1u << 20;

}

FileDigestStatus sha256_file(const std::filesystem::path& path, const Progress& progress, Progress::Stage stage, std::uint64_t& done, std::uint64_t total,
                             std::string& sha256) {
    const auto in_file = files_provider().open_read(path, std::ios::binary);
    std::istream& in = *in_file;

    if (!in) {
        return FileDigestStatus::Unreadable;
    }

    Sha256 hash;
    std::vector<char> buffer(chunk_bytes);

    while (true) {
        if (progress.cancelled()) {
            return FileDigestStatus::Cancelled;
        }

        in.read(buffer.data(), std::streamsize(buffer.size()));
        const std::streamsize got = in.gcount();

        if (got > 0) {
            hash.update(buffer.data(), std::size_t(got));
            done += std::uint64_t(got);
            progress.report(stage, done, total);
        }

        if (!in) {
            if (in.eof()) {
                break;
            }

            return FileDigestStatus::Unreadable;
        }
    }

    sha256 = hash.hex_digest();
    return FileDigestStatus::Ok;
}

}
