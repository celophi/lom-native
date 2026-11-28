#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <filesystem>
#include <fstream>

using namespace lom::launcher::test;

namespace {
struct ChangeGameFilesLocationSpecification : LauncherSpec {
    /**
     * @brief Make the picker choose "Games drive".
     */
    ChangeGameFilesLocationSpecification() {
        games_root_choice = shown(chosen);
    }

    std::filesystem::path chosen = profile / "Games drive";
};

}

LOM_SPEC(ChangeGameFilesLocationSpecification, "Changing the game files folder should show and remember the new folder") {
    open_launcher();

    press("changeDestinationButton");

    EXPECT_EQ(text_of("destinationFolder"), shown(chosen));
    EXPECT_EQ(settings_member("games_root").as_string(), chosen.u8string());
}

LOM_SPEC(ChangeGameFilesLocationSpecification, "Changing the game files folder should keep the previous folder when the chosen one is not writable") {
    *disk::write(chosen) << "not a folder";
    open_launcher();

    press("changeDestinationButton");

    EXPECT_EQ(text_of("destinationFolder"), shown(data_directory / "games"));
    EXPECT_THAT(text_of("banner"), StartsWithText("The launcher could not write to the game files folder:"));
}

LOM_SPEC(ChangeGameFilesLocationSpecification, "Changing the game files folder should remember a folder whose name has non-English letters") {
    const std::filesystem::path accented = profile / std::filesystem::u8path("Spiele \xc3\xbc");
    games_root_choice = shown(accented);
    open_launcher();

    press("changeDestinationButton");

    EXPECT_EQ(text_of("destinationFolder"), shown(accented));
    EXPECT_EQ(settings_member("games_root").as_string(), accented.u8string());
}
