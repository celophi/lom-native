#ifndef LOM_LAUNCHER_RELEASE_HISTORY_H
#define LOM_LAUNCHER_RELEASE_HISTORY_H

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QUrl>

#include <optional>
#include <vector>

namespace lom::launcher {
struct ReleaseHistory {
    struct Release {
        QString version;
        QString notes;
        QDate published;
        QUrl page;
    };

    /**
     * @brief Read a GitHub releases list.
     * @param body The list, as JSON.
     * @param error Receives why when the list can't be read; may be null.
     * @return The stable releases, newest version first; drafts, prereleases and odd tags are left out.
     */
    static std::optional<std::vector<Release>> interpret(const QByteArray& body, QString* error);

    /**
     * @brief Compare two versions.
     * @return Negative, zero or positive; versions that don't parse count as older than any that do.
     */
    static int compare(const QString& a, const QString& b);

    /**
     * @brief Whether a version is a plain release, with no prerelease part or build details.
     * @param version Version text.
     */
    static bool is_release(const QString& version);
};

}

#endif
