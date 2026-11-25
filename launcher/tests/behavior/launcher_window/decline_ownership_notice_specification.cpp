#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_printers.h"

#include <filesystem>

using namespace lom::launcher::test;

namespace {
struct DeclineOwnershipNoticeSpecification : LauncherSpec {
    /**
     * @brief Start from a profile that has never acknowledged the notice.
     */
    DeclineOwnershipNoticeSpecification() {
        std::filesystem::remove(config_directory / "launcher.json");
    }
};

}

LOM_SPEC(DeclineOwnershipNoticeSpecification, "Quitting at the ownership notice should close the launcher without remembering an acknowledgement") {
    open_launcher();

    press("noticeQuitButton");

    EXPECT_FALSE(launcher().isVisible());
    EXPECT_FALSE(std::filesystem::exists(config_directory / "launcher.json"));
}
