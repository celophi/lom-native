#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "window/ui_text.h"
#include "spec.h"
#include "spec_printers.h"

#include <QPushButton>

#include <filesystem>

using namespace lom::launcher::test;

namespace {
struct GameExitsSpecification : LauncherSpec {
    /**
     * @brief Set up the game from an extracted folder.
     */
    GameExitsSpecification() {
        given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"));
    }
};

}

LOM_SPEC(GameExitsSpecification, "The game exiting should show the launcher again, ready to play") {
    open_launcher();
    press("playButton");

    the_game_exits(0);

    EXPECT_TRUE(launcher().isVisible());
    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(launcher().findChild<QPushButton*>("playButton")->text(), "Play");
}

LOM_SPEC(GameExitsSpecification, "The game exiting should explain that the game stopped unexpectedly when it exits with an error") {
    open_launcher();
    press("playButton");

    the_game_exits(3);

    EXPECT_TRUE(launcher().isVisible());
    EXPECT_EQ(heading(), "The game stopped unexpectedly");
    EXPECT_EQ(text_of("problemBody"),
              "The game exited with an error (code 3).\n\nDetails were written to:\n" + shown(data_directory / "logs" / "game-20261015-120000.log"));
}

LOM_SPEC(GameExitsSpecification, "The game exiting should explain that the graphics driver can't run it when its graphics didn't start") {
    open_launcher();
    press("playButton");

    the_game_exits(4);

    EXPECT_TRUE(launcher().isVisible());
    EXPECT_EQ(heading(), "The game could not start its graphics");
    EXPECT_EQ(text_of("problemBody"),
              QString(lom::launcher::ui_text::graphics_unavailable_body).arg(shown(data_directory / "logs" / "game-20261015-120000.log")));
}

LOM_SPEC(GameExitsSpecification, "The game exiting should call a crash a crash even when its code matches the graphics one") {
    open_launcher();
    press("playButton");

    the_game_crashes(4);

    EXPECT_EQ(heading(), "The game stopped unexpectedly");
}

LOM_SPEC(GameExitsSpecification, "The game exiting should bring the launcher back when I closed it during play") {
    open_launcher();
    press("playButton");
    launcher().close();
    settle();

    the_game_exits(0);

    EXPECT_TRUE(launcher().isVisible());
    EXPECT_EQ(heading(), "Ready to play");
}
