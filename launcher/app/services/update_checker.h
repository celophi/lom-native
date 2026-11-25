#ifndef LOM_LAUNCHER_UPDATE_CHECKER_H
#define LOM_LAUNCHER_UPDATE_CHECKER_H

#include <QObject>
#include <QString>
#include <QUrl>

namespace lom::launcher {

class UpdateFeed;

class UpdateChecker : public QObject {
    Q_OBJECT

  public:
    struct Result {
        enum class Status { NotConfigured, UpToDate, UpdateAvailable, CouldNotCheck };

        Status status = Status::CouldNotCheck;
        QString latest_version;
        QString release_notes;
        QUrl release_page;
        QString error;
    };

    /**
     * @brief Create a checker.
     * @param feed Release feed; null when update checks are not configured. Must outlive the checker.
     * @param current_version This build's version.
     * @param parent Owner.
     */
    UpdateChecker(UpdateFeed* feed, QString current_version, QObject* parent);

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

  signals:
    /**
     * @brief Report a completed check.
     * @param result Outcome.
     */
    void checked(const lom::launcher::UpdateChecker::Result& result);

  private:
    UpdateFeed* feed_ = nullptr;
    QString current_version_;
    bool checking_ = false;
};

}

#endif
