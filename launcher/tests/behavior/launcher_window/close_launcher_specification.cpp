#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_printers.h"

using namespace lom::launcher::test;

namespace {
struct CloseLauncherSpecification : LauncherSpec {
    /**
     * @brief Set up the game from an extracted folder.
     */
    CloseLauncherSpecification() {
        given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"));
    }
};

}

LOM_SPEC(CloseLauncherSpecification, "Closing the launcher during play should keep the game running") {
    open_launcher();
    press("playButton");

    launcher().close();
    settle();

    EXPECT_TRUE(launcher().game_running());
    EXPECT_FALSE(launcher().isVisible());
}
