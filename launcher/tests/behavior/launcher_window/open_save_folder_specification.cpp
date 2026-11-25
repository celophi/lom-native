#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_printers.h"

#include <filesystem>
#include <vector>

using namespace lom::launcher::test;

namespace {
struct OpenSaveFolderSpecification : LauncherSpec {
    /**
     * @brief Set up the game from an extracted folder.
     */
    OpenSaveFolderSpecification() {
        given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"));
    }
};

}

LOM_SPEC(OpenSaveFolderSpecification, "Opening the save folder should show it in the file manager") {
    open_launcher();

    press("openSavesButton");

    EXPECT_EQ(opened_folders, std::vector<QString>{shown(data_directory / "cards")});
    EXPECT_TRUE(std::filesystem::is_directory(data_directory / "cards" / "slot0"));
}
