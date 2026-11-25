#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_printers.h"

#include <QToolButton>

using namespace lom::launcher::test;

namespace {
struct ToggleThemeSpecification : LauncherSpec {
    /**
     * @brief Press the theme toggle.
     */
    void press_the_theme_toggle() {
        launcher().findChild<QToolButton*>("themeToggle")->click();
        settle();
    }
};

}

LOM_SPEC(ToggleThemeSpecification, "Switching to dark mode should remember the choice") {
    open_launcher();

    press_the_theme_toggle();

    EXPECT_EQ(settings_member("theme").as_string(), "dark");
}

LOM_SPEC(ToggleThemeSpecification, "Switching to dark mode should offer light mode next") {
    open_launcher();

    press_the_theme_toggle();

    EXPECT_EQ(launcher().findChild<QToolButton*>("themeToggle")->text(), "Light mode");
}
