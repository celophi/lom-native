#include "launcher_spec.h"
#include "window/app_paths.h"
#include "spec.h"

#include <filesystem>
#include <fstream>

using namespace lom::launcher;
using namespace lom::launcher::test;

namespace {
struct PortableModeSpecification : LauncherSpec {};

}

LOM_SPEC(PortableModeSpecification, "Opening the launcher should keep everything in the user folder beside it when portable.txt is there") {
    *disk::write(program_directory / "portable.txt") << "";

    const AppPaths paths = AppPaths::for_program(program_directory);

    const std::filesystem::path user = program_directory / "user";
    EXPECT_TRUE(disk::is_directory(user));
    EXPECT_EQ(paths.settings_file(), user / "launcher.json");
    EXPECT_EQ(paths.data_directory(), user);
    EXPECT_EQ(paths.default_games_root(), user / "games");
    EXPECT_EQ(paths.default_save_root("us"), user / "cards");
    EXPECT_EQ(paths.logs_directory(), user / "logs");
}

LOM_SPEC(PortableModeSpecification, "Opening the launcher should use the user's own folders when there's no portable.txt") {
    const AppPaths paths = AppPaths::for_program(program_directory);

    EXPECT_FALSE(disk::exists(program_directory / "user"));
    EXPECT_NE(paths.settings_file().parent_path(), program_directory / "user");
    EXPECT_NE(paths.data_directory(), program_directory / "user");
}
