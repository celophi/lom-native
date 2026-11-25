#include "lom/launcher/disc/installation.h"

#include "lom/launcher/common/json.h"

namespace lom::launcher {

namespace {

constexpr char key_kind[] = "kind";
constexpr char key_data_path[] = "data_path";
constexpr char key_release[] = "release";
constexpr char key_manifest_sha256[] = "manifest_sha256";
constexpr char key_source_sha1[] = "source_sha1";
constexpr char key_installation_id[] = "installation_id";
constexpr char kind_managed[] = "managed";
constexpr char kind_external[] = "external";

}

Json Installation::to_json() const {
    Json value;
    value[key_kind] = kind == Kind::Managed ? kind_managed : kind_external;
    value[key_data_path] = data_path.u8string();
    value[key_release] = release_id;
    value[key_manifest_sha256] = manifest_sha256;
    value[key_source_sha1] = source_sha1;
    value[key_installation_id] = installation_id;
    return value;
}

std::optional<Installation> Installation::from_json(const Json& value) {
    const Json* kind = value.find(key_kind);
    const Json* data_path = value.find(key_data_path);
    const Json* release = value.find(key_release);

    if (kind == nullptr || data_path == nullptr || release == nullptr || data_path->as_string().empty() || release->as_string().empty()) {
        return std::nullopt;
    }

    Installation installation;

    if (kind->as_string() == kind_managed) {
        installation.kind = Kind::Managed;
    } else if (kind->as_string() == kind_external) {
        installation.kind = Kind::External;
    } else {
        return std::nullopt;
    }

    installation.data_path = std::filesystem::u8path(data_path->as_string());
    installation.release_id = release->as_string();

    if (const Json* manifest = value.find(key_manifest_sha256)) {
        installation.manifest_sha256 = manifest->as_string();
    }

    if (const Json* source = value.find(key_source_sha1)) {
        installation.source_sha1 = source->as_string();
    }

    if (const Json* id = value.find(key_installation_id)) {
        installation.installation_id = id->as_string();
    }

    return installation;
}

}
