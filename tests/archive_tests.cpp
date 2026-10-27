#include "core/archive.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <archive.h>
#include <archive_entry.h>

using namespace rolly;

class ArchiveTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;

    QString write(const QString &relative, const QByteArray &bytes) {
        const auto path = dir_.filePath(relative);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write(bytes);
        return path;
    }
    static QStringList names(const Listing &listing) {
        QStringList result;
        for (const auto &entry : listing.entries)
            result.append(entry.path);
        result.sort();
        return result;
    }

private slots:
    void roundTripEveryWritableFormat_data() {
        QTest::addColumn<QString>("name");
        for (const auto &suffix : creatableSuffixes())
            QTest::newRow(qPrintable(suffix)) << "sample" + suffix;
    }
    void roundTripEveryWritableFormat() {
        QFETCH(QString, name);
        const auto folder = dir_.filePath("src-" + name);
        write("src-" + name + "/a.txt", "alpha");
        write("src-" + name + "/sub/b.txt", "bravo");
        const auto archive = dir_.filePath(name);
        QFile::remove(archive);
        addToArchive(archive, {folder}, {}, {}, {});
        const auto base = QFileInfo(folder).fileName();
        QCOMPARE(names(readArchive(archive)), (QStringList{base, base + "/a.txt", base + "/sub", base + "/sub/b.txt"}));

        const auto out = dir_.filePath("out-" + name);
        extractArchive(archive, out, {}, {}, {});
        QFile extracted(out + "/" + base + "/sub/b.txt");
        QVERIFY(extracted.open(QIODevice::ReadOnly));
        QCOMPARE(extracted.readAll(), QByteArray("bravo"));
    }
    void addReplacesAndRemoveDeletes() {
        const auto archive = dir_.filePath("edit.zip");
        const auto first = write("edit/one.txt", "1");
        addToArchive(archive, {first}, "docs", {}, {});
        write("edit/one.txt", "11");
        addToArchive(archive, {first, write("edit/two.txt", "2")}, "docs", {}, {});
        QCOMPARE(names(readArchive(archive)), (QStringList{"docs/one.txt", "docs/two.txt"}));
        removeFromArchive(archive, {"docs"}, {}, {});
        QVERIFY(readArchive(archive).entries.isEmpty());
    }
    void selectedExtractionKeepsOnlySelection() {
        const auto archive = dir_.filePath("pick.zip");
        addToArchive(archive, {write("pick/a.txt", "a"), write("pick/b.txt", "b")}, {}, {}, {});
        const auto out = dir_.filePath("pick-out");
        extractArchive(archive, out, {"a.txt"}, {}, {});
        QVERIFY(QFile::exists(out + "/a.txt"));
        QVERIFY(!QFile::exists(out + "/b.txt"));
    }
    void refusesPathTraversal() {
        const auto evil = dir_.filePath("evil.tar");
        archive *writer = archive_write_new();
        archive_write_set_format_pax_restricted(writer);
        archive_write_open_filename(writer, QFile::encodeName(evil).constData());
        archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, "../escaped.txt");
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_size(entry, 4);
        archive_write_header(writer, entry);
        archive_write_data(writer, "evil", 4);
        archive_entry_free(entry);
        archive_write_close(writer);
        archive_write_free(writer);

        const auto out = dir_.filePath("jail/inner");
        QVERIFY_EXCEPTION_THROWN(extractArchive(evil, out, {}, {}, {}), std::runtime_error);
        QVERIFY(!QFile::exists(dir_.filePath("jail/escaped.txt")));
    }
    void neverWritesThroughEscapingSymlink() {
        const auto evil = dir_.filePath("link.tar");
        archive *writer = archive_write_new();
        archive_write_set_format_pax_restricted(writer);
        archive_write_open_filename(writer, QFile::encodeName(evil).constData());
        archive_entry *link = archive_entry_new();
        archive_entry_set_pathname(link, "evil");
        archive_entry_set_filetype(link, AE_IFLNK);
        archive_entry_set_symlink(link, "..");
        archive_entry_set_perm(link, 0777);
        archive_write_header(writer, link);
        archive_entry_free(link);
        archive_entry *file = archive_entry_new();
        archive_entry_set_pathname(file, "evil/x.txt");
        archive_entry_set_filetype(file, AE_IFREG);
        archive_entry_set_perm(file, 0644);
        archive_entry_set_size(file, 3);
        archive_write_header(writer, file);
        archive_write_data(writer, "bad", 3);
        archive_entry_free(file);
        archive_write_close(writer);
        archive_write_free(writer);
        const auto out = dir_.filePath("linkjail/inner");
        // Either refused or neutralised, but never written outside the destination.
        try { extractArchive(evil, out, {}, {}, {}); } catch (const std::runtime_error &) {}
        QVERIFY(!QFileInfo(out + "/evil").isSymLink());
        QVERIFY(!QFile::exists(dir_.filePath("linkjail/x.txt")));
    }
    void cancellationLeavesOriginalUntouched() {
        const auto archive = dir_.filePath("keep.zip");
        const auto file = write("keep/k.txt", QByteArray(300000, 'k'));
        addToArchive(archive, {file}, {}, {}, {});
        const auto before = QFileInfo(archive).size();
        QVERIFY_EXCEPTION_THROWN(addToArchive(archive, {file}, "x", {}, [](qint64, qint64, const QString &) { return false; }), Cancelled);
        QCOMPARE(QFileInfo(archive).size(), before);
        QCOMPARE(QDir(dir_.path()).entryList({".keep.zip.rolly-*"}, QDir::Files | QDir::Hidden).size(), 0);
    }
    void addKeepsExistingContent_data() {
        QTest::addColumn<QString>("name");
        for (const auto &suffix : creatableSuffixes())
            QTest::newRow(qPrintable(suffix)) << "grow" + suffix;
    }
    void addKeepsExistingContent() {
        QFETCH(QString, name);
        const auto archive = dir_.filePath(name);
        QFile::remove(archive);
        const QByteArray big(200000, 'q');
        addToArchive(archive, {write("grow/" + name + "-big.bin", big)}, {}, {}, {});
        addToArchive(archive, {write("grow/" + name + "-more.txt", "more")}, "later", {}, {});
        const auto out = dir_.filePath("grow-out-" + name);
        extractArchive(archive, out, {}, {}, {});
        QFile kept(out + "/" + name + "-big.bin");
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), big);
        QVERIFY(QFile::exists(out + "/later/" + name + "-more.txt"));
    }
    void readOnlyFormatsAreNotModifiable() {
        QVERIFY(!canModify("x.rar"));
        QVERIFY(canModify("x.tar.zst"));
        QVERIFY(looksLikeArchive("X.RAR"));
    }
};

QTEST_MAIN(ArchiveTests)
#include "archive_tests.moc"
