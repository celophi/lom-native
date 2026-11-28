#include <gtest/gtest.h>

#include <QApplication>

/**
 * @brief Run the providers' contract tests in a Qt application, which the dialogs and the desktop need. It draws
 *        offscreen with no platform theme, so Qt shows its own dialogs, which the tests answer, and never the system's,
 *        which would open real windows on the desktop of whoever runs it.
 * @param argc Argument count.
 * @param argv GoogleTest options.
 * @return Zero when every check passed.
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QPA_PLATFORMTHEME", "");
    QApplication application(argc, argv);
    return RUN_ALL_TESTS();
}
