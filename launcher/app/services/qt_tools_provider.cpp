#include "services/qt_tools_provider.h"

#include <QProcess>

namespace lom::launcher {

ToolsProvider::Run QtToolsProvider::run(const QString& program, const QStringList& arguments, int timeout_ms) {
    Run result;
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);

    if (!process.waitForStarted()) {
        result.error = process.errorString();
        return result;
    }

    result.started = true;

    if (!process.waitForFinished(timeout_ms)) {
        process.kill();
        process.waitForFinished();
        return result;
    }

    result.finished = true;
    result.crashed = process.exitStatus() != QProcess::NormalExit;
    result.exit_code = process.exitCode();
    result.output = QString::fromLocal8Bit(process.readAll());
    return result;
}

bool QtToolsProvider::start_detached(const QString& program, const QStringList& arguments) {
    return QProcess::startDetached(program, arguments);
}

}
