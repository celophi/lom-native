#ifndef LOM_LAUNCHER_RELEASE_CATALOG_H
#define LOM_LAUNCHER_RELEASE_CATALOG_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lom::launcher {

class Json;

class ReleaseCatalog {
  public:
    struct Release {
        std::string id;
        std::string region;
        std::string serial;
        std::string executable;
        std::string display_name;
        std::string raw_sha1;
        std::uint64_t raw_bytes = 0;
        std::string raw_layout;
        std::filesystem::path manifest;
        std::string runtime;
        std::string card_prefix;
    };

    enum class Match { Supported, Unknown };

    static constexpr int schema_version = 1;

    /**
     * @brief Load the catalog; manifest paths resolve relative to its directory.
     * @param path supported-discs.json.
     * @param error Receives a description on failure; may be null.
     * @return True when every release entry is well formed.
     */
    bool load(const std::filesystem::path& path, std::string* error);

    /**
     * @brief Load the catalog from parsed JSON.
     * @param document Parsed catalog.
     * @param base_directory Directory for relative manifest paths.
     * @param error Receives a description on failure; may be null.
     * @return True when every release entry is well formed.
     */
    bool from_json(const Json& document, const std::filesystem::path& base_directory, std::string* error);

    /**
     * @brief Find the release with an exact raw image identity.
     * @param layout Image layout from DiscImage::layout(); only raw layouts can match.
     * @param sha1 Image SHA-1.
     * @param bytes Image length.
     * @return The release, or null when the identity is not a supported one.
     */
    const Release* identify(const std::string& layout, const std::string& sha1, std::uint64_t bytes) const;

    /**
     * @brief Find a release by its boot executable, only to explain an unverified input.
     * @param executable Executable name, any case.
     * @return The release, or null.
     */
    const Release* by_executable(const std::string& executable) const;

    /**
     * @brief Find the release whose saves use a memory-card file name.
     * @param filename Card file name.
     * @return The release, or null when no release's prefix matches.
     */
    const Release* by_card_filename(const std::string& filename) const;

    /**
     * @brief Find a release by identifier.
     * @param id Release identifier.
     * @return The release, or null.
     */
    const Release* by_id(const std::string& id) const;

    /**
     * @brief Report every release.
     * @return Releases in catalog order.
     */
    const std::vector<Release>& releases() const;

    /**
     * @brief Report the decomp revision the identities came from.
     * @return Revision hash.
     */
    const std::string& upstream_revision() const;

  private:
    std::vector<Release> releases_;
    std::string upstream_revision_;
};

}

#endif
