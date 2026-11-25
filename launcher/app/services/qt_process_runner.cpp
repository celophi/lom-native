#include "services/qt_process_runner.h"

#include <QProcessEnvironment>

namespace lom::launcher {

QtProcessRunner::QtProcessRunner(QObject* parent) : ProcessRunner(parent) {
    connect(&process_, &QProcess::started, this, &ProcessRunner::started);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit failed(process_.errorString());
        }
    });
    connect(&process_, &QProcess::finished, this, [this](int exit_code, QProcess::ExitStatus status) {
        emit finished(exit_code, status == QProcess::CrashExit);
    });
}

QtProcessRunner::~QtProcessRunner() {
    if (process_.state() != QProcess::NotRunning) {
        process_.disconnect(this);
        process_.waitForFinished(-1);
    }
}

void QtProcessRunner::start(const Request& request) {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    for (auto variable = request.environment.cbegin(); variable != request.environment.cend(); ++variable) {
        environment.insert(variable.key(), variable.value());
    }

    process_.setProcessEnvironment(environment);
    process_.setProgram(request.program);
    process_.setArguments(request.arguments);
    process_.setWorkingDirectory(request.working_directory);
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.setStandardOutputFile(request.log_file, QIODevice::Append);
    process_.setStandardInputFile(QProcess::nullDevice());
    process_.start();
}

bool QtProcessRunner::running() const {
    return process_.state() != QProcess::NotRunning;
}

}
