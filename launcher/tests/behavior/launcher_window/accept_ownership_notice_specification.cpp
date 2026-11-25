#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_printers.h"

#include <QPushButton>

#include <filesystem>
#include <fstream>

using namespace lom::launcher::test;

namespace {
struct AcceptOwnershipNoticeSpecification : LauncherSpec {
    /**
     * @brief Start from a profile that has never acknowledged the notice.
     */
    AcceptOwnershipNoticeSpecification() {
        std::filesystem::remove(config_directory / "launcher.json");
    }
};

}

LOM_SPEC(AcceptOwnershipNoticeSpecification, "Accepting the ownership notice should remember the acknowledgement") {
    open_launcher();

    press("noticeAcceptButton");

    EXPECT_TRUE(json_is(read_json(config_directory / "launcher.json"),
                        R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "system", "games_root": "", "last_image_directory": "",
                          "check_updates_on_startup": true, "active_installation": null, "installations": {}, "save_roots": {},
                          "last_seen_version": "0.4.0", "record_sessions": false, "use_remaster": true, "remaster_root": "", "language": ""})"));
}

LOM_SPEC(AcceptOwnershipNoticeSpecification, "Accepting the ownership notice should continue to choosing a game source") {
    open_launcher();

    press("noticeAcceptButton");

    EXPECT_EQ(heading(), "Welcome");
}

LOM_SPEC(AcceptOwnershipNoticeSpecification,
         "Accepting the ownership notice should explain the failure and not claim it was saved when settings cannot be written") {
    std::filesystem::remove_all(config_directory);
    std::ofstream(config_directory) << "a file where the folder should be";

    open_launcher();

    press("noticeAcceptButton");

    EXPECT_EQ(heading(), "Your answer could not be saved");
    EXPECT_EQ(launcher().findChild<QPushButton*>("problemPrimaryButton")->text(), "Try again");
    EXPECT_EQ(launcher().findChild<QPushButton*>("problemSecondaryButton")->text(), "Quit");
}
