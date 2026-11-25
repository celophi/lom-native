#ifndef LOM_LAUNCHER_FILE_DIGEST_H
#define LOM_LAUNCHER_FILE_DIGEST_H

#include "lom/launcher/common/progress.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace lom::launcher {
enum class FileDigestStatus { Ok, Unreadable, Cancelled };

/**
 * @brief Stream a file through SHA-256, adding its bytes to shared stage progress.
 * @param path File to hash.
 * @param progress Progress sink and cancellation.
 * @param stage Stage to report.
 * @param done Running byte count for the stage; advanced by the bytes read.
 * @param total Total bytes for the stage.
 * @param sha256 Receives the lowercase digest on Ok.
 * @return Ok, Unreadable or Cancelled.
 */
FileDigestStatus sha256_file(const std::filesystem::path& path, const Progress& progress, Progress::Stage stage, std::uint64_t& done, std::uint64_t total,
                             std::string& sha256);

}

#endif
