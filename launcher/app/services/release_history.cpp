#include "services/release_history.h"

#include "lom/launcher/common/json.h"
#include "lom/launcher/common/semantic_version.h"

#include <QDateTime>

#include <algorithm>

namespace lom::launcher {

namespace {

constexpr char key_tag[] = "tag_name";
constexpr char key_body[] = "body";
constexpr char key_html_url[] = "html_url";
constexpr char key_published[] = "published_at";
constexpr char key_draft[] = "draft";
constexpr char key_prerelease[] = "prerelease";

/**
 * @brief Read a boolean member that may be missing.
 * @param release Release object.
 * @param key Member name.
 */
bool flag(const Json& release, const char* key) {
    const Json* value = release.find(key);
    return value != nullptr && value->as_bool(false);
}

/**
 * @brief Read a string member that may be missing.
 * @param release Release object.
 * @param key Member name.
 */
QString text(const Json& release, const char* key) {
    const Json* value = release.find(key);
    return value == nullptr ? QString() : QString::fromStdString(value->as_string());
}

}

std::optional<std::vector<ReleaseHistory::Release>> ReleaseHistory::interpret(const QByteArray& body, QString* error) {
    std::string problem;
    const auto document = Json::parse(body.toStdString(), &problem);

    if (!document || document->type() != Json::Type::Array) {
        if (error != nullptr) {
            *error = QStringLiteral("the release list was malformed");
        }

        return std::nullopt;
    }

    std::vector<Release> releases;

    for (const Json& entry : document->as_array()) {
        if (entry.type() != Json::Type::Object || flag(entry, key_draft) || flag(entry, key_prerelease)) {
            continue;
        }

        const auto version = SemanticVersion::parse(text(entry, key_tag).toStdString());

        if (!version || version->is_prerelease()) {
            continue;
        }

        Release release;
        release.version = QString::fromStdString(version->to_string());
        release.notes = text(entry, key_body);
        release.published = QDateTime::fromString(text(entry, key_published), Qt::ISODate).toUTC().date();
        const QUrl page(text(entry, key_html_url));

        if (page.scheme() == QLatin1String("https")) {
            release.page = page;
        }

        releases.push_back(std::move(release));
    }

    std::stable_sort(releases.begin(), releases.end(), [](const Release& a, const Release& b) {
        return compare(a.version, b.version) > 0;
    });

    return releases;
}

int ReleaseHistory::compare(const QString& a, const QString& b) {
    const auto left = SemanticVersion::parse(a.toStdString());
    const auto right = SemanticVersion::parse(b.toStdString());

    if (!left || !right) {
        return left ? 1 : right ? -1 : 0;
    }

    return left->compare(*right);
}

bool ReleaseHistory::is_release(const QString& version) {
    const auto parsed = SemanticVersion::parse(version.toStdString());
    return parsed && !parsed->is_prerelease() && !version.contains(QLatin1Char('+'));
}

}
