#include <QApplication>
#include <QLocale>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdio>

namespace {
constexpr char in_memory_display[] = "offscreen";

}

/**
 * @brief Run the specifications.
 * @param argc Argument count.
 * @param argv GoogleTest options, such as --gtest_filter="*Play should*".
 * @return Zero when every selected specification passed.
 */
int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", in_memory_display);
    qputenv("QT_QPA_PLATFORMTHEME", "");
    ::testing::InitGoogleMock(&argc, argv);
    QApplication application(argc, argv);

    if (QGuiApplication::platformName() != QLatin1String(in_memory_display)) {
        std::fprintf(stderr, "lom-launcher-specs: refusing to run on the \"%s\" display; specifications use only the in-memory display\n",
                     qPrintable(QGuiApplication::platformName()));
        return 2;
    }

    QApplication::setApplicationName(QStringLiteral("lom-native-launcher-specs"));
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    return RUN_ALL_TESTS();
}
