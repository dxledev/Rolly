#include "core/archive.h"
#include "core/storage.h"
#include "ui/window.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace rolly;

class UiTests : public QObject {
    Q_OBJECT
private slots:
    void opensArchiveAndBrowsesFolders() {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("box/inner"));
        QFile(dir.filePath("box/inner/note.txt")).open(QIODevice::WriteOnly);
        QFile(dir.filePath("box/top.txt")).open(QIODevice::WriteOnly);
        const auto archive = dir.filePath("box.zip");
        addToArchive(archive, {dir.filePath("box")}, {}, {}, {});

        Window window(dir.filePath("cfg"), ensureTheme(dir.filePath("cfg/theme.json")));
        window.show();
        window.openArchive(archive);
        QTRY_COMPARE(window.archivePath(), archive);
        QTRY_VERIFY(window.model()->rowCount() == 1);
        QCOMPARE(window.model()->pathAt(0), QString("box"));
        QVERIFY(window.model()->setDirectory("box"));
        QCOMPARE(window.model()->rowCount(), 2);
        QVERIFY(window.model()->isDirectoryAt(0));
        QCOMPARE(window.model()->parentDirectory(), QString());
    }
    void followsThemeFileLive() {
        QTemporaryDir dir;
        const auto path = dir.filePath("cfg/theme.json");
        Window window(dir.filePath("cfg"), ensureTheme(path));
        window.show();
        auto theme = setColors(loadTheme(path), {"background=#e00e1e36"});
        saveTheme(path, theme);
        QTRY_VERIFY_WITH_TIMEOUT(window.styleSheet().contains("rgba(14,30,54,224)"), 3000);
    }
};

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
