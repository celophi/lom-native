#ifndef LOM_LAUNCHER_PROCESS_RUNNER_H
#define LOM_LAUNCHER_PROCESS_RUNNER_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

namespace lom::launcher {
class ProcessRunner : public QObject {
    Q_OBJECT

  public:
    struct Request {
        QString program;
        QStringList arguments;
        QMap<QString, QString> environment;
        QString working_directory;
        QString log_file;
    };

    /**
     * @brief Create a runner.
     * @param parent Owner.
     */
    explicit ProcessRunner(QObject* parent);

    virtual void start(const Request& request) = 0;

    virtual bool running() const = 0;

  signals:
    /**
     * @brief The process started.
     */
    void started();

    /**
     * @brief The process could not start.
     * @param message Reason.
     */
    void failed(const QString& message);

    /**
     * @brief The process ended.
     * @param exit_code Exit status.
     * @param crashed Whether it ended abnormally.
     */
    void finished(int exit_code, bool crashed);
};

}

#endif
