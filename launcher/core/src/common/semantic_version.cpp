#include "lom/launcher/common/semantic_version.h"

#include <cstdlib>
#include <vector>

namespace lom::launcher {

namespace {
constexpr std::size_t max_number_digits = 9;

/**
 * @brief Parse a non-empty decimal number without leading zeros.
 * @param text Digits.
 * @param value Receives the number.
 * @return False when malformed.
 */
bool parse_number(const std::string& text, unsigned long long& value) {
    if (text.empty() || text.size() > max_number_digits || (text.size() > 1 && text[0] == '0')) {
        return false;
    }

    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }

    value = std::strtoull(text.c_str(), nullptr, 10);
    return true;
}

/**
 * @brief Split text on a separator.
 * @param text Text to split.
 * @param separator Separator character.
 * @return Parts, including empty ones.
 */
std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts(1);

    for (const char c : text) {
        if (c == separator) {
            parts.emplace_back();
        } else {
            parts.back().push_back(c);
        }
    }

    return parts;
}

/**
 * @brief Compare prerelease labels by SemVer precedence.
 * @param a First label.
 * @param b Second label.
 * @return Negative, zero or positive.
 */
int compare_prerelease(const std::string& a, const std::string& b) {
    const std::vector<std::string> left = split(a, '.');
    const std::vector<std::string> right = split(b, '.');

    for (std::size_t i = 0; i < left.size() && i < right.size(); i++) {
        unsigned long long left_number = 0;
        unsigned long long right_number = 0;
        const bool left_numeric = parse_number(left[i], left_number);
        const bool right_numeric = parse_number(right[i], right_number);

        if (left_numeric && right_numeric) {
            if (left_number != right_number) {
                return left_number < right_number ? -1 : 1;
            }
        } else if (left_numeric != right_numeric) {
            return left_numeric ? -1 : 1;
        } else if (left[i] != right[i]) {
            return left[i] < right[i] ? -1 : 1;
        }
    }

    if (left.size() == right.size()) {
        return 0;
    }

    return left.size() < right.size() ? -1 : 1;
}

}

std::optional<SemanticVersion> SemanticVersion::parse(const std::string& text) {
    std::string core = text;

    if (!core.empty() && (core[0] == 'v' || core[0] == 'V')) {
        core.erase(0, 1);
    }

    const std::size_t build = core.find('+');

    if (build != std::string::npos) {
        core.erase(build);
    }

    SemanticVersion version;
    const std::size_t dash = core.find('-');

    if (dash != std::string::npos) {
        version.prerelease_ = core.substr(dash + 1);
        core.erase(dash);

        if (version.prerelease_.empty()) {
            return std::nullopt;
        }
    }

    const std::vector<std::string> numbers = split(core, '.');

    if (numbers.size() != 3 || !parse_number(numbers[0], version.major_) || !parse_number(numbers[1], version.minor_) ||
        !parse_number(numbers[2], version.patch_)) {
        return std::nullopt;
    }

    return version;
}

int SemanticVersion::compare(const SemanticVersion& other) const {
    if (major_ != other.major_) {
        return major_ < other.major_ ? -1 : 1;
    }

    if (minor_ != other.minor_) {
        return minor_ < other.minor_ ? -1 : 1;
    }

    if (patch_ != other.patch_) {
        return patch_ < other.patch_ ? -1 : 1;
    }

    if (prerelease_.empty() || other.prerelease_.empty()) {
        if (prerelease_.empty() == other.prerelease_.empty()) {
            return 0;
        }

        return prerelease_.empty() ? 1 : -1;
    }

    return compare_prerelease(prerelease_, other.prerelease_);
}

bool SemanticVersion::is_prerelease() const {
    return !prerelease_.empty();
}

std::string SemanticVersion::to_string() const {
    std::string text = std::to_string(major_) + "." + std::to_string(minor_) + "." + std::to_string(patch_);
    return prerelease_.empty() ? text : text + "-" + prerelease_;
}

}
