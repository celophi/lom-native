#include "services/system_desktop_provider.h"

#include <gtest/gtest.h>

#include <QDesktopServices>
#include <QObject>
#include <QUrl>

#include <vector>

namespace {

using namespace lom::launcher;

class OpenedUrls : public QObject {
    Q_OBJECT

  public slots:
    /**
     * @brief Note a URL the system was asked to open, in place of opening it.
     * @param url The URL.
     */
    void open(const QUrl& url) {
        urls.push_back(url);
    }

  public:
    std::vector<QUrl> urls;
};

class DesktopProviderContract : public ::testing::Test {
  protected:
    /**
     * @brief Have the desktop hand the test the URLs it's asked to open, for files and web pages, instead of a file
     *        manager or a browser.
     */
    DesktopProviderContract() {
        QDesktopServices::setUrlHandler(QStringLiteral("file"), &opened, "open");
        QDesktopServices::setUrlHandler(QStringLiteral("https"), &opened, "open");
    }

    /**
     * @brief Put the desktop's own handlers back.
     */
    ~DesktopProviderContract() override {
        QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
    }

    OpenedUrls opened;
    SystemDesktopProvider desktop;
};

TEST_F(DesktopProviderContract, opens_a_folder_as_its_local_file_url) {
    desktop.open_folder(QStringLiteral("/home/player/saves"));

    ASSERT_EQ(opened.urls.size(), 1u);
    EXPECT_EQ(opened.urls.front(), QUrl::fromLocalFile(QStringLiteral("/home/player/saves")));
}

TEST_F(DesktopProviderContract, opens_a_web_page_as_it_is_given) {
    desktop.open_url(QUrl(QStringLiteral("https://github.com/celophi/lom-native/issues/new")));

    ASSERT_EQ(opened.urls.size(), 1u);
    EXPECT_EQ(opened.urls.front(), QUrl(QStringLiteral("https://github.com/celophi/lom-native/issues/new")));
}

}

#include "desktop_provider_contract.moc"
