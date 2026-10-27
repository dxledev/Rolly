#include "archive.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <memory>
#include <unistd.h>

namespace rolly {
namespace {

constexpr size_t blockSize = 64 * 1024;

struct ReadDeleter { void operator()(archive *a) const { archive_read_free(a); } };
struct WriteDeleter { void operator()(archive *a) const { archive_write_free(a); } };
struct EntryDeleter { void operator()(archive_entry *e) const { archive_entry_free(e); } };
using Reader = std::unique_ptr<archive, ReadDeleter>;
using Writer = std::unique_ptr<archive, WriteDeleter>;
using EntryPtr = std::unique_ptr<archive_entry, EntryDeleter>;

[[noreturn]] void fail(const QString &message) {
    throw std::runtime_error(message.toStdString());
}

[[noreturn]] void failWith(archive *a, const QString &fallback) {
    const char *text = archive_error_string(a);
    const QString message = text ? QString::fromLocal8Bit(text) : fallback;
    if (message.contains("assphrase", Qt::CaseInsensitive))
        throw NeedsPassphrase();
    fail(message);
}

void check(archive *a, int result, const QString &fallback) {
    if (result < ARCHIVE_WARN)
        failWith(a, fallback);
}

QString entryName(archive_entry *entry) {
    if (const char *utf8 = archive_entry_pathname_utf8(entry))
        return QString::fromUtf8(utf8);
    return QString::fromLocal8Bit(archive_entry_pathname(entry));
}

Reader openReader(const QString &path, const QString &passphrase) {
    Reader reader(archive_read_new());
    archive_read_support_filter_all(reader.get());
    archive_read_support_format_all(reader.get());
    archive_read_support_format_raw(reader.get());
    if (!passphrase.isEmpty())
        archive_read_add_passphrase(reader.get(), passphrase.toUtf8().constData());
    if (archive_read_open_filename(reader.get(), QFile::encodeName(path).constData(), blockSize) != ARCHIVE_OK)
        failWith(reader.get(), "Cannot open " + path);
    return reader;
}

QString lowerName(const QString &path) {
    return QFileInfo(path).fileName().toLower();
}

bool hasSuffix(const QString &name, const QString &suffix) {
    return name.endsWith(suffix);
}

void configureWriter(archive *writer, const QString &path, const QString &passphrase) {
    const auto name = lowerName(path);
    int result = ARCHIVE_OK;
    if (hasSuffix(name, ".zip")) {
        result = archive_write_set_format_zip(writer);
        if (!passphrase.isEmpty()) {
            archive_write_set_options(writer, "zip:encryption=aes256");
            archive_write_set_passphrase(writer, passphrase.toUtf8().constData());
        }
    } else if (hasSuffix(name, ".7z")) {
        result = archive_write_set_format_7zip(writer);
    } else if (hasSuffix(name, ".tar") || hasSuffix(name, ".tar.gz") || hasSuffix(name, ".tgz")
               || hasSuffix(name, ".tar.bz2") || hasSuffix(name, ".tbz2") || hasSuffix(name, ".tar.xz")
               || hasSuffix(name, ".txz") || hasSuffix(name, ".tar.zst") || hasSuffix(name, ".tzst")) {
        result = archive_write_set_format_pax_restricted(writer);
        if (result == ARCHIVE_OK) {
            if (hasSuffix(name, ".gz") || hasSuffix(name, ".tgz"))
                result = archive_write_add_filter_gzip(writer);
            else if (hasSuffix(name, ".bz2") || hasSuffix(name, ".tbz2"))
                result = archive_write_add_filter_bzip2(writer);
            else if (hasSuffix(name, ".xz") || hasSuffix(name, ".txz"))
                result = archive_write_add_filter_xz(writer);
            else if (hasSuffix(name, ".zst") || hasSuffix(name, ".tzst"))
                result = archive_write_add_filter_zstd(writer);
        }
    } else {
        fail("Rolly cannot modify this archive type. Supported: " + creatableSuffixes().join(", "));
    }
    check(writer, result, "This archive format is not supported here");
}

bool matches(const QString &path, const QStringList &selection) {
    for (const auto &item : selection)
        if (path == item || path.startsWith(item + '/'))
            return true;
    return false;
}

bool unsafeComponent(const QString &path) {
    if (path.startsWith('/'))
        return true;
    for (const auto &part : path.split('/'))
        if (part == "..")
            return true;
    return false;
}

struct Source {
    QString disk;
    QString archive;
    qint64 size = 0;
};

QVector<Source> collectSources(const QStringList &sources, const QString &prefix) {
    QVector<Source> result;
    const auto cleanPrefix = normalizedEntryPath(prefix);
    const auto join = [&](const QString &name) { return cleanPrefix.isEmpty() ? name : cleanPrefix + '/' + name; };
    for (const auto &source : sources) {
        const QFileInfo info(source);
        if (!info.exists() && !info.isSymLink())
            fail("Cannot find " + source);
        const auto base = join(info.fileName());
        result.append({info.absoluteFilePath(), base, info.isFile() && !info.isSymLink() ? info.size() : 0});
        if (info.isDir() && !info.isSymLink()) {
            const QDir root(info.absoluteFilePath());
            QDirIterator iterator(root.absolutePath(), QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                  QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                const auto child = iterator.next();
                const QFileInfo childInfo(child);
                result.append({child, base + '/' + root.relativeFilePath(child),
                               childInfo.isFile() && !childInfo.isSymLink() ? childInfo.size() : 0});
            }
        }
    }
    return result;
}

struct Pacer {
    const Progress &progress;
    qint64 total;
    qint64 done = 0;
    qint64 reported = -1;
    void tick(qint64 bytes, const QString &current) {
        done += bytes;
        if (done - reported < 128 * 1024 && reported >= 0 && done < total)
            return;
        reported = done;
        if (progress && !progress(done, total, current))
            throw Cancelled();
    }
};

void copyData(archive *reader, archive *writer, Pacer &pacer, const QString &name, bool toDisk) {
    const void *buffer;
    size_t size;
    la_int64_t offset;
    for (;;) {
        const int result = archive_read_data_block(reader, &buffer, &size, &offset);
        if (result == ARCHIVE_EOF)
            return;
        check(reader, result, "Cannot read " + name);
        // Only the disk writer understands sparse offsets; archive writers take a plain stream.
        const auto written = toDisk ? archive_write_data_block(writer, buffer, size, offset)
                                    : archive_write_data(writer, buffer, size);
        if (written < (toDisk ? ARCHIVE_WARN : 0))
            failWith(writer, "Cannot write " + name);
        pacer.tick(qint64(size), name);
    }
}

void writeSource(archive *writer, archive *disk, const Source &source, Pacer &pacer) {
    EntryPtr entry(archive_entry_new());
    archive_entry_copy_pathname(entry.get(), QFile::encodeName(source.disk).constData());
    check(disk, archive_read_disk_entry_from_file(disk, entry.get(), -1, nullptr), "Cannot read " + source.disk);
    archive_entry_update_pathname_utf8(entry.get(), source.archive.toUtf8().constData());
    if (archive_write_header(writer, entry.get()) < ARCHIVE_WARN)
        failWith(writer, "Cannot add " + source.archive);
    if (archive_entry_filetype(entry.get()) == AE_IFREG && archive_entry_size(entry.get()) > 0) {
        const int descriptor = ::open(QFile::encodeName(source.disk).constData(), O_RDONLY | O_CLOEXEC);
        if (descriptor < 0)
            fail("Cannot read " + source.disk);
        std::unique_ptr<char[]> buffer(new char[blockSize]);
        for (;;) {
            const auto count = ::read(descriptor, buffer.get(), blockSize);
            if (count < 0) {
                ::close(descriptor);
                fail("Cannot read " + source.disk);
            }
            if (count == 0)
                break;
            if (archive_write_data(writer, buffer.get(), size_t(count)) < 0) {
                ::close(descriptor);
                failWith(writer, "Cannot add " + source.archive);
            }
            try {
                pacer.tick(count, source.archive);
            } catch (...) {
                ::close(descriptor);
                throw;
            }
        }
        ::close(descriptor);
    } else {
        pacer.tick(0, source.archive);
    }
}

void rewrite(const QString &path, const QString &passphrase, const QStringList &removals,
             const QVector<Source> &additions, const Progress &progress, qint64 totalHint) {
    const QFileInfo target(path);
    const bool existed = target.exists();
    QSet<QString> replaced;
    for (const auto &source : additions)
        replaced.insert(source.archive);

    qint64 total = totalHint;
    for (const auto &source : additions)
        total += source.size;
    Pacer pacer{progress, total};

    const QString temporary = target.absolutePath() + "/." + target.fileName() + ".rolly-"
        + QString::number(::getpid()) + ".tmp";
    QFile::remove(temporary);
    struct Cleanup {
        QString path;
        bool armed = true;
        ~Cleanup() { if (armed) QFile::remove(path); }
    } cleanup{temporary};

    {
        Writer writer(archive_write_new());
        configureWriter(writer.get(), path, passphrase);
        if (archive_write_open_filename(writer.get(), QFile::encodeName(temporary).constData()) != ARCHIVE_OK)
            failWith(writer.get(), "Cannot create " + temporary);

        if (existed) {
            auto reader = openReader(path, passphrase);
            archive_entry *entry = nullptr;
            for (;;) {
                const int status = archive_read_next_header(reader.get(), &entry);
                if (status == ARCHIVE_EOF)
                    break;
                check(reader.get(), status, "Cannot read " + path);
                const auto name = normalizedEntryPath(entryName(entry));
                if (name.isEmpty() || matches(name, removals) || replaced.contains(name))
                    continue;
                if (archive_write_header(writer.get(), entry) < ARCHIVE_WARN)
                    failWith(writer.get(), "Cannot write " + name);
                if (archive_entry_filetype(entry) == AE_IFREG && archive_entry_size(entry) > 0)
                    copyData(reader.get(), writer.get(), pacer, name, false);
                else
                    pacer.tick(0, name);
            }
        }

        Reader disk(archive_read_disk_new());
        archive_read_disk_set_standard_lookup(disk.get());
        for (const auto &source : additions)
            writeSource(writer.get(), disk.get(), source, pacer);

        check(writer.get(), archive_write_close(writer.get()), "Cannot finish " + path);
    }
    if (existed)
        QFile::setPermissions(temporary, QFile::permissions(path));
    if (::rename(QFile::encodeName(temporary).constData(), QFile::encodeName(path).constData()) != 0)
        fail("Cannot replace " + path);
    cleanup.armed = false;
}

}

QString normalizedEntryPath(QString path) {
    while (path.startsWith("./"))
        path.remove(0, 2);
    while (path.startsWith('/'))
        path.remove(0, 1);
    while (path.endsWith('/'))
        path.chop(1);
    return path == "." ? QString() : path;
}

QStringList creatableSuffixes() {
    return {".zip", ".7z", ".tar", ".tar.gz", ".tar.xz", ".tar.bz2", ".tar.zst"};
}

bool canModify(const QString &path) {
    const auto name = lowerName(path);
    for (const auto &suffix : creatableSuffixes())
        if (name.endsWith(suffix))
            return true;
    return name.endsWith(".tgz") || name.endsWith(".txz") || name.endsWith(".tbz2") || name.endsWith(".tzst");
}

bool looksLikeArchive(const QString &path) {
    static const QStringList suffixes{
        ".zip", ".7z", ".rar", ".tar", ".gz", ".tgz", ".bz2", ".tbz2", ".xz", ".txz", ".zst", ".tzst",
        ".lz", ".lzma", ".z", ".cpio", ".iso", ".jar", ".war", ".apk", ".deb", ".rpm", ".cab", ".lha", ".lzh", ".xpi"
    };
    const auto name = lowerName(path);
    for (const auto &suffix : suffixes)
        if (name.endsWith(suffix))
            return true;
    return false;
}

Listing readArchive(const QString &path, const QString &passphrase) {
    auto reader = openReader(path, passphrase);
    Listing listing;
    archive_entry *entry = nullptr;
    for (;;) {
        const int status = archive_read_next_header(reader.get(), &entry);
        if (status == ARCHIVE_EOF)
            break;
        check(reader.get(), status, "Cannot read " + path);
        Entry item;
        item.path = normalizedEntryPath(entryName(entry));
        if (item.path.isEmpty())
            continue;
        const auto type = archive_entry_filetype(entry);
        item.directory = type == AE_IFDIR;
        item.symlink = type == AE_IFLNK;
        item.size = archive_entry_size_is_set(entry) ? archive_entry_size(entry) : 0;
        if (archive_entry_mtime_is_set(entry))
            item.modified = QDateTime::fromSecsSinceEpoch(archive_entry_mtime(entry));
        item.encrypted = archive_entry_is_encrypted(entry);
        listing.encrypted = listing.encrypted || item.encrypted;
        listing.entries.append(item);
        check(reader.get(), archive_read_data_skip(reader.get()), "Cannot read " + path);
    }
    listing.format = QString::fromLocal8Bit(archive_format_name(reader.get()));
    return listing;
}

void extractArchive(const QString &path, const QString &destination, const QStringList &selection,
                    const QString &passphrase, const Progress &progress, qint64 totalHint) {
    if (!QDir().mkpath(destination))
        fail("Cannot create " + destination);
    QStringList wanted;
    for (const auto &item : selection)
        wanted.append(normalizedEntryPath(item));

    auto reader = openReader(path, passphrase);
    Writer disk(archive_write_disk_new());
    archive_write_disk_set_options(disk.get(), ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM
        | ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_UNLINK);
    archive_write_disk_set_standard_lookup(disk.get());

    // Entry names are checked below before the destination is prepended; resolving the
    // destination first keeps the symlink guard focused on what the archive itself creates.
    const QString root = QFileInfo(destination).canonicalFilePath();
    Pacer pacer{progress, totalHint};
    archive_entry *entry = nullptr;
    for (;;) {
        const int status = archive_read_next_header(reader.get(), &entry);
        if (status == ARCHIVE_EOF)
            break;
        check(reader.get(), status, "Cannot read " + path);
        const auto name = normalizedEntryPath(entryName(entry));
        if (name.isEmpty() || (!wanted.isEmpty() && !matches(name, wanted)))
            continue;
        if (unsafeComponent(name))
            fail("Refusing unsafe path in archive: " + name);
        archive_entry_update_pathname_utf8(entry, (root + '/' + name).toUtf8().constData());
        if (const char *link = archive_entry_hardlink_utf8(entry)) {
            const auto linkName = normalizedEntryPath(QString::fromUtf8(link));
            if (unsafeComponent(linkName))
                fail("Refusing unsafe link in archive: " + linkName);
            archive_entry_update_hardlink_utf8(entry, (root + '/' + linkName).toUtf8().constData());
        }
        if (archive_write_header(disk.get(), entry) < ARCHIVE_WARN)
            failWith(disk.get(), "Cannot extract " + name);
        if (archive_entry_size(entry) > 0 && archive_entry_filetype(entry) == AE_IFREG)
            copyData(reader.get(), disk.get(), pacer, name, true);
        else
            pacer.tick(0, name);
        check(disk.get(), archive_write_finish_entry(disk.get()), "Cannot extract " + name);
    }
}

void addToArchive(const QString &path, const QStringList &sources, const QString &prefix,
                  const QString &passphrase, const Progress &progress) {
    if (sources.isEmpty())
        fail("Nothing to add");
    const auto additions = collectSources(sources, prefix);
    for (const auto &source : additions)
        if (QFileInfo(source.disk).absoluteFilePath() == QFileInfo(path).absoluteFilePath())
            fail("An archive cannot contain itself");
    rewrite(path, passphrase, {}, additions, progress, 0);
}

void removeFromArchive(const QString &path, const QStringList &selection, const QString &passphrase,
                       const Progress &progress, qint64 totalHint) {
    QStringList removals;
    for (const auto &item : selection)
        removals.append(normalizedEntryPath(item));
    if (removals.isEmpty())
        fail("Nothing to delete");
    rewrite(path, passphrase, removals, {}, progress, totalHint);
}

}
