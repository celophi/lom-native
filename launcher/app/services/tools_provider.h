#ifndef LOM_LAUNCHER_TOOLS_PROVIDER_H
#define LOM_LAUNCHER_TOOLS_PROVIDER_H

#include <QString>
#include <QStringList>

namespace lom::launcher {
class ToolsProvider {
  public:
    struct Run {
        bool started = false;
        bool finished = false;
        bool crashed = false;
        int exit_code = -1;
        QString output;
        QString error;
    };

    /**
     * @brief Destroy it.
     */
    virtual ~ToolsProvider() = default;

    /**
     * @brief Run a program and wait for it, killing it when it takes too long.
     * @param program The program.
     * @param arguments Its arguments.
     * @param timeout_ms How long to wait.
     * @return Whether it started and finished, how it ended, what it wrote (its output and errors together), and why
     *         it didn't start.
     * @note The launcher's provider for running other programs (the system's tar, hdiutil, ditto) and starting itself
     *       again: the one place it does, as thin as it can be.
     */
    virtual Run run(const QString& program, const QStringList& arguments, int timeout_ms) = 0;

    /**
     * @brief Start a program on its own, not waiting for it, as the launcher does to restart into an update.
     * @param program The program.
     * @param arguments Its arguments.
     * @return Whether it started.
     */
    virtual bool start_detached(const QString& program, const QStringList& arguments) = 0;
};

}

#endif
