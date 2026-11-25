#ifndef LOM_LAUNCHER_SEMANTIC_VERSION_H
#define LOM_LAUNCHER_SEMANTIC_VERSION_H

#include <optional>
#include <string>

namespace lom::launcher {
class SemanticVersion {
  public:
    /**
     * @brief Parse a version or release tag.
     * @param text Version such as "1.2.3", "v1.2.3" or "0.4.0-dev".
     * @return The version, or nothing when malformed.
     */
    static std::optional<SemanticVersion> parse(const std::string& text);

    /**
     * @brief Compare with another version, ranking a prerelease before its release.
     * @param other Version to compare with.
     * @return Negative, zero or positive as this is older, equal or newer.
     */
    int compare(const SemanticVersion& other) const;

    /**
     * @brief Report whether this is a prerelease, such as a development build.
     * @return True when a prerelease label is present.
     */
    bool is_prerelease() const;

    /**
     * @brief Format the version without a "v" prefix.
     * @return Version text.
     */
    std::string to_string() const;

  private:
    unsigned long long major_ = 0;
    unsigned long long minor_ = 0;
    unsigned long long patch_ = 0;
    std::string prerelease_;
};

}

#endif
