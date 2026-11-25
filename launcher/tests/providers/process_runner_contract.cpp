#include "services/qt_process_runner.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>

namespace {

using namespace lom::launcher;

constexpr int timeout_ms = 10000;

constexpr int poll_ms = 5;

constexpr char running_ms[] = "300";

constexpr int chosen_exit_code = 3;

class QtProcessRunnerContract : public ::testing::Test {
  protected:
    /**
     * @brief Prepare a request for the stand-in game and record the runner's signals.
     */
    QtProcessRunnerContract() {
        request.program = QStringLiteral(LOM_FAKE_GAME);
        request.working_directory = directory.path();
        request.log_file = path("game.log");
        request.environment.insert("LOM_FAKE_GAME_RECORD", path("record.txt"));

        QObject::connect(&runner, &ProcessRunner::started, [this]() {
            started = true;
            running_when_started = runner.running();
        });
        QObject::connect(&runner, &ProcessRunner::finished, [this](int code, bool did_crash) {
            finished = true;
            exit_code = code;
            crashed = did_crash;
        });
        QObject::connect(&runner, &ProcessRunner::failed, [this](const QString& message) {
            failure = message;
        });
    }

    /**
     * @brief Name a file in the temporary folder.
     * @param name File name.
     * @return Its path.
     */
    QString path(const QString& name) const {
        return directory.filePath(name);
    }

    /**
     * @brief Read a file in the temporary folder.
     * @param name File name.
     * @return Its text, or an empty string when it cannot be read.
     */
    QString read(const QString& name) const {
        QFile file(path(name));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }

    /**
     * @brief Start the request and process events until the runner reports an end or the timeout passes.
     */
    void run() {
        runner.start(request);
        running_after_start = runner.running();
        QElapsedTimer timer;
        timer.start();

        while (!finished && failure.isEmpty() && timer.elapsed() < timeout_ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            QThread::msleep(poll_ms);
        }
    }

    QTemporaryDir directory;
    QtProcessRunner runner{nullptr};
    ProcessRunner::Request request;

    bool running_after_start = false;
    bool started = false;
    bool running_when_started = false;
    bool finished = false;
    int exit_code = -1;
    bool crashed = false;
    QString failure;
};

TEST_F(QtProcessRunnerContract, passes_the_arguments_and_adds_variables_to_the_launchers_environment) {
    request.arguments = {path("game data")};
    request.environment.insert("LOM_CARD_ROOT", path("cards"));
    qputenv("LOM_FAKE_GAME_INHERITED", "from the launcher");

    run();
    qunsetenv("LOM_FAKE_GAME_INHERITED");

    ASSERT_TRUE(finished);
    const QStringList record = read("record.txt").split('\n');
    EXPECT_EQ(record.value(0).toStdString(), ("data=" + path("game data")).toStdString());
    EXPECT_EQ(record.value(1).toStdString(), ("card_root=" + path("cards")).toStdString());
    EXPECT_EQ(record.value(2).toStdString(), "inherited=from the launcher");
}

TEST_F(QtProcessRunnerContract, starts_the_program_in_the_working_directory) {
    QDir(directory.path()).mkdir("work");
    request.working_directory = path("work");

    run();

    ASSERT_TRUE(finished);
    const QString recorded = read("record.txt").split('\n').value(3).section('=', 1);
    EXPECT_EQ(QFileInfo(recorded).canonicalFilePath().toStdString(), QFileInfo(path("work")).canonicalFilePath().toStdString());
}

TEST_F(QtProcessRunnerContract, appends_standard_output_and_error_to_the_log_file) {
    QFile earlier(request.log_file);
    ASSERT_TRUE(earlier.open(QIODevice::WriteOnly));
    earlier.write("earlier run\n");
    earlier.close();

    run();

    ASSERT_TRUE(finished);
    const QString log = read("game.log");
    EXPECT_TRUE(log.startsWith("earlier run\n")) << log.toStdString();
    EXPECT_TRUE(log.contains("fake game: standard output")) << log.toStdString();
    EXPECT_TRUE(log.contains("fake game: standard error")) << log.toStdString();
}

TEST_F(QtProcessRunnerContract, reports_start_then_the_exit_code_of_a_normal_exit) {
    request.environment.insert("LOM_FAKE_GAME_EXIT", QString::number(chosen_exit_code));

    run();

    EXPECT_TRUE(started);
    EXPECT_TRUE(finished);
    EXPECT_EQ(exit_code, chosen_exit_code);
    EXPECT_FALSE(crashed);
    EXPECT_TRUE(failure.isEmpty());
}

TEST_F(QtProcessRunnerContract, reports_a_crash) {
    request.environment.insert("LOM_FAKE_GAME_CRASH", "1");

    run();

    EXPECT_TRUE(started);
    EXPECT_TRUE(finished);
    EXPECT_TRUE(crashed);
}

TEST_F(QtProcessRunnerContract, reports_running_from_start_until_the_program_ends) {
    request.environment.insert("LOM_FAKE_GAME_SLEEP_MS", running_ms);

    run();

    EXPECT_TRUE(running_after_start);
    EXPECT_TRUE(running_when_started);
    EXPECT_TRUE(finished);
    EXPECT_FALSE(runner.running());
}

TEST_F(QtProcessRunnerContract, reports_a_program_that_cannot_start) {
    request.program = path("missing-program");

    run();

    EXPECT_FALSE(failure.isEmpty());
    EXPECT_FALSE(started);
    EXPECT_FALSE(runner.running());
}

}

/**
 * @brief Run the contract checks.
 * @param argc Argument count.
 * @param argv GoogleTest options.
 * @return Zero when every check passed.
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    QCoreApplication application(argc, argv);
    return RUN_ALL_TESTS();
}
