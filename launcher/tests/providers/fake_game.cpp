#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

namespace {
constexpr char output_line[] = "fake game: standard output";

constexpr char error_line[] = "fake game: standard error";

#ifdef _WIN32
constexpr unsigned int access_violation = 0xC0000005;
#endif

/**
 * @brief Read an environment variable.
 * @param name Variable.
 * @return Its value, or an empty string when unset.
 */
std::string variable(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

/**
 * @brief End the process abnormally, as a crash would.
 */
[[noreturn]] void crash() {
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), access_violation);
#else
    std::raise(SIGKILL);
#endif
    std::abort();
}

}

/**
 * @brief Record the launch, write both streams, and end as the environment asks.
 * @param argc Argument count.
 * @param argv Arguments; argv[1] is the game data folder.
 * @return LOM_FAKE_GAME_EXIT, or zero.
 */
int main(int argc, char** argv) {
    const std::string record = variable("LOM_FAKE_GAME_RECORD");

    if (!record.empty()) {
        if (FILE* out = std::fopen(record.c_str(), "wb")) {
            std::fprintf(out, "data=%s\ncard_root=%s\ninherited=%s\nworking_directory=%s\n", argc > 1 ? argv[1] : "", variable("LOM_CARD_ROOT").c_str(),
                         variable("LOM_FAKE_GAME_INHERITED").c_str(), std::filesystem::current_path().generic_string().c_str());
            std::fclose(out);
        }
    }

    std::fprintf(stdout, "%s\n", output_line);
    std::fprintf(stderr, "%s\n", error_line);
    std::fflush(stdout);
    std::fflush(stderr);

    const std::string sleep_ms = variable("LOM_FAKE_GAME_SLEEP_MS");

    if (!sleep_ms.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(sleep_ms.c_str())));
    }

    if (!variable("LOM_FAKE_GAME_CRASH").empty()) {
        crash();
    }

    const std::string exit_code = variable("LOM_FAKE_GAME_EXIT");
    return exit_code.empty() ? 0 : std::atoi(exit_code.c_str());
}
