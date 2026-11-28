#include "services/native_file_dialogs_provider.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>

namespace {

using namespace lom::launcher;

class FileDialogsProviderContract : public ::testing::Test {
  protected:
    /**
     * @brief Answer the next dialog that opens, as the player would, once it's showing.
     * @param answer What to do with it.
     */
    void when_it_opens(std::function<void(QFileDialog*)> answer) {
        QTimer::singleShot(0, [answer]() {
            auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            ASSERT_NE(dialog, nullptr);
            answer(dialog);
        });
    }

    /**
     * @brief Make a file in the test's folder.
     * @param name Its name.
     * @return Its path.
     */
    QString make_file(const QString& name) {
        QFile file(folder.filePath(name));
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        return file.fileName();
    }

    QTemporaryDir folder;
    NativeFileDialogsProvider dialogs;
};

TEST_F(FileDialogsProviderContract, gives_the_file_chosen_from_a_dialog_with_its_title_and_folder) {
    const QString image = make_file(QStringLiteral("legend.cue"));
    QString title, directory;
    when_it_opens([&](QFileDialog* dialog) {
        title = dialog->windowTitle();
        directory = dialog->directory().absolutePath();
        dialog->selectFile(image);
        static_cast<QDialog*>(dialog)->accept();
    });

    const QString chosen = dialogs.choose_file(nullptr, QStringLiteral("Choose an image"), folder.path(), QStringLiteral("Images (*.cue)"));

    EXPECT_EQ(chosen, image);
    EXPECT_EQ(title, QStringLiteral("Choose an image"));
    EXPECT_EQ(directory, QDir(folder.path()).absolutePath());
}

TEST_F(FileDialogsProviderContract, gives_nothing_when_a_file_dialog_is_cancelled) {
    when_it_opens([](QFileDialog* dialog) {
        dialog->reject();
    });

    const QString chosen = dialogs.choose_file(nullptr, QStringLiteral("Choose an image"), folder.path(), QStringLiteral("Images (*.cue)"));

    EXPECT_TRUE(chosen.isEmpty());
}

TEST_F(FileDialogsProviderContract, gives_the_folder_chosen_from_a_folder_dialog) {
    QDir(folder.path()).mkdir(QStringLiteral("saves"));
    const QString saves = QDir(folder.path()).absoluteFilePath(QStringLiteral("saves"));
    when_it_opens([&](QFileDialog* dialog) {
        dialog->setDirectory(saves);
        static_cast<QDialog*>(dialog)->accept();
    });

    const QString chosen = dialogs.choose_folder(nullptr, QStringLiteral("Choose a folder"), folder.path());

    EXPECT_EQ(QDir(chosen).absolutePath(), saves);
}

TEST_F(FileDialogsProviderContract, gives_nothing_when_a_folder_dialog_is_cancelled) {
    when_it_opens([](QFileDialog* dialog) {
        dialog->reject();
    });

    const QString chosen = dialogs.choose_folder(nullptr, QStringLiteral("Choose a folder"), folder.path());

    EXPECT_TRUE(chosen.isEmpty());
}

}
