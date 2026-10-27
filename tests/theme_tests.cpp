#include "core/theme.h"
#include "core/theme_source.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace rolly;

class ThemeTests : public QObject {
    Q_OBJECT
private slots:
    void alphaComesFirst() {
        const auto theme = setColors(presetTheme("lofi"), {"background=#e00e1e36"});
        const auto color = theme.color("background");
        QCOMPARE(color.alpha(), 0xe0);
        QCOMPARE(color.name(QColor::HexRgb), QString("#0e1e36"));
        QCOMPARE(theme.toJson().value("colors").toObject().value("background").toString(), QString("#e00e1e36"));
    }
    void qtRgbaMatchesHex() {
        const auto theme = setColors(presetTheme("lofi"), {"background=Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)"});
        QCOMPARE(theme.color("background").name(QColor::HexArgb), QString("#e00e1e36"));
    }
    void flatteningDropsOnlyAlpha() {
        const auto flat = opaqueTheme(setColors(presetTheme("lofi"), {"background=#e00e1e36"}));
        QCOMPARE(flat.color("background").name(QColor::HexArgb), QString("#ff0e1e36"));
    }
    void opaqueColorsStaySixDigits() {
        const auto theme = setColors(presetTheme("lofi"), {"accent=#123456"});
        QCOMPARE(theme.toJson().value("colors").toObject().value("accent").toString(), QString("#123456"));
    }
    void rejectsGarbage() {
        QVERIFY_EXCEPTION_THROWN(setColors(presetTheme("lofi"), {"accent=nope"}), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(setColors(presetTheme("lofi"), {"unknown=#fff"}), std::invalid_argument);
    }
    void followsDirectoryWithQmlColors() {
        QTemporaryDir dir;
        QFile file(dir.filePath("Colors.qml"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("pragma Singleton\nSingleton {\n"
                   "readonly property color background: Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)\n"
                   "readonly property color foreground: \"#eaeff5\"\nreadonly property color primary: \"#E37B66\"\n}\n");
        file.close();
        const auto theme = followTheme("file", dir.path());
        QCOMPARE(theme.color("background").name(QColor::HexArgb), QString("#e00e1e36"));
        QCOMPARE(theme.color("accent").name(), QString("#e37b66"));
    }
};

QTEST_MAIN(ThemeTests)
#include "theme_tests.moc"
