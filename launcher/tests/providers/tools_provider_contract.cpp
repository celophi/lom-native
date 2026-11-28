#include "services/qt_tools_provider.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

namespace {

using namespace lom::launcher;

constexpr int timeout_ms = 10000;
constexpr int poll_ms = 5;
constexpr int short_timeout_ms = 300;

class ToolsProviderContract : public ::testing::Test {
  protected:
    /**
     * @brief Clear what the stand-in program reads from its environment.
     */
    ToolsProviderContract() {
        clear();
    }

    /**
     * @brief Leave the environment as it was.
     */
    ~ToolsProviderContract() override {
        clear();
    }

    /**
     * @brief Remove the stand-in program's variables.
     */
    static void clear() {
        qunsetenv("LOM_FAKE_GAME_EXIT");
        qunsetenv("LOM_FAKE_GAME_CRASH");
        qunsetenv("LOM_FAKE_GAME_SLEEP_MS");
        qunsetenv("LOM_FAKE_GAME_RECORD");
    }

    QtToolsProvider tools;
    QTemporaryDir directory;
};

}

TEST_F(ToolsProviderContract, runs_a_program_and_hands_back_how_it_ended_and_what_it_wrote) {
    qputenv("LOM_FAKE_GAME_EXIT", "3");

    const ToolsProvider::Run run = tools.run(QStringLiteral(LOM_FAKE_GAME), {QStringLiteral("an argument")}, timeout_ms);

    EXPECT_TRUE(run.started);
    EXPECT_TRUE(run.finished);
    EXPECT_FALSE(run.crashed);
    EXPECT_EQ(run.exit_code, 3);
    EXPECT_TRUE(run.output.contains(QStringLiteral("fake game: standard output")));
    EXPECT_TRUE(run.output.contains(QStringLiteral("fake game: standard error")));
}

TEST_F(ToolsProviderContract, passes_the_arguments) {
    qputenv("LOM_FAKE_GAME_RECORD", directory.filePath(QStringLiteral("record.txt")).toLocal8Bit());

    tools.run(QStringLiteral(LOM_FAKE_GAME), {QStringLiteral("the first argument")}, timeout_ms);

    QFile record(directory.filePath(QStringLiteral("record.txt")));
    ASSERT_TRUE(record.open(QIODevice::ReadOnly));
    EXPECT_TRUE(record.readAll().startsWith("data=the first argument\n"));
}

TEST_F(ToolsProviderContract, reports_a_crash) {
    qputenv("LOM_FAKE_GAME_CRASH", "1");

    const ToolsProvider::Run run = tools.run(QStringLiteral(LOM_FAKE_GAME), {}, timeout_ms);

    EXPECT_TRUE(run.started);
    EXPECT_TRUE(run.finished);
    EXPECT_TRUE(run.crashed);
}

TEST_F(ToolsProviderContract, stops_a_program_that_takes_too_long) {
    qputenv("LOM_FAKE_GAME_SLEEP_MS", "5000");
    QElapsedTimer waited;
    waited.start();

    const ToolsProvider::Run run = tools.run(QStringLiteral(LOM_FAKE_GAME), {}, short_timeout_ms);

    EXPECT_TRUE(run.started);
    EXPECT_FALSE(run.finished);
    EXPECT_LT(waited.elapsed(), 4000);
}

TEST_F(ToolsProviderContract, says_why_a_program_cannot_start) {
    const ToolsProvider::Run run = tools.run(directory.filePath(QStringLiteral("no such program")), {}, timeout_ms);

    EXPECT_FALSE(run.started);
    EXPECT_FALSE(run.finished);
    EXPECT_FALSE(run.error.isEmpty());
}

TEST_F(ToolsProviderContract, starts_a_program_on_its_own_without_waiting_for_it) {
    qputenv("LOM_FAKE_GAME_RECORD", directory.filePath(QStringLiteral("record.txt")).toLocal8Bit());

    const bool started = tools.start_detached(QStringLiteral(LOM_FAKE_GAME), {QStringLiteral("after an update")});

    EXPECT_TRUE(started);
    QElapsedTimer waited;
    waited.start();
    QFile record(directory.filePath(QStringLiteral("record.txt")));

    while (!(record.exists() && record.size() > 0) && waited.elapsed() < timeout_ms) {
        QThread::msleep(poll_ms);
    }

    ASSERT_TRUE(record.open(QIODevice::ReadOnly));
    EXPECT_TRUE(record.readAll().startsWith("data=after an update\n"));
}

TEST_F(ToolsProviderContract, says_when_a_program_cannot_be_started_on_its_own) {
    const bool started = tools.start_detached(directory.filePath(QStringLiteral("no such program")), {});

    EXPECT_FALSE(started);
}
