#ifndef LOM_LAUNCHER_UPDATE_CHECKER_H
#define LOM_LAUNCHER_UPDATE_CHECKER_H

#include "services/update_feed_provider.h"

#include <QMap>
#include <QObject>
#include <QString>
#include <QUrl>

namespace lom::launcher {


class UpdateChecker : public QObject {
    Q_OBJECT

  public:
    struct Result {
        enum class Status { NotConfigured, UpToDate, UpdateAvailable, CouldNotCheck };

        Status status = Status::CouldNotCheck;
        QString latest_version;
        QString release_notes;
        QUrl release_page;
        QMap<QString, QUrl> assets;
        QString error;
    };

    /**
     * @brief Create a checker.
     * @param feed Release feed; null when update checks are not configured. Must outlive the checker.
     * @param current_version This build's version.
     * @param repository The repository the releases are in.
     * @param parent Owner.
     */
    UpdateChecker(UpdateFeedProvider* feed, QString current_version, QString repository, QObject* parent);

    /**
     * @brief Start one asynchronous check; ignored while a check is running.
     */
    void check();

    /**
     * @brief Interpret a GitHub "latest release" document.
     * @param body Release JSON.
     * @param current_version This build's version.
     * @return The result.
     */
    static Result interpret(const QByteArray& body, const QString& current_version);

    /**
     * @brief Say why the release feed couldn't be read, for the player.
     * @param response The feed's response.
     * @param repository The repository the releases are in.
     * @return The reason.
     */
    static QString feed_error(const UpdateFeedProvider::Response& response, const QString& repository);

  signals:
    /**
     * @brief Report a completed check.
     * @param result Outcome.
     */
    void checked(const lom::launcher::UpdateChecker::Result& result);

  private:
    UpdateFeedProvider* feed_ = nullptr;
    QString current_version_;
    QString repository_;
    bool checking_ = false;
};

}

#endif
