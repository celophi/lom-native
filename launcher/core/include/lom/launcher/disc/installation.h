#ifndef LOM_LAUNCHER_INSTALLATION_H
#define LOM_LAUNCHER_INSTALLATION_H

#include <filesystem>
#include <optional>
#include <string>

namespace lom::launcher {

class Json;

struct Installation {
    enum class Kind { Managed, External };

    Kind kind = Kind::Managed;
    std::filesystem::path data_path;
    std::string release_id;
    std::string manifest_sha256;
    std::string source_sha1;
    std::string installation_id;

    /**
     * @brief Serialize for launcher settings.
     * @return JSON object.
     */
    Json to_json() const;

    /**
     * @brief Read a serialized installation.
     * @param value JSON object from to_json().
     * @return The installation, or nothing when incomplete.
     */
    static std::optional<Installation> from_json(const Json& value);
};

}

#endif
