#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <stdexcept>

namespace rolly {

struct Entry {
    QString path;
    qint64 size = 0;
    QDateTime modified;
    bool directory = false;
    bool symlink = false;
    bool encrypted = false;
};

struct Listing {
    QVector<Entry> entries;
    QString format;
    bool encrypted = false;
};

struct NeedsPassphrase : std::runtime_error {
    NeedsPassphrase() : std::runtime_error("This archive is password protected") {}
};

struct Cancelled : std::runtime_error {
    Cancelled() : std::runtime_error("Cancelled") {}
};

// Return false to cancel. Called often; implementations should be cheap.
using Progress = std::function<bool(qint64 done, qint64 total, const QString &current)>;

Listing readArchive(const QString &path, const QString &passphrase = {});
void extractArchive(const QString &path, const QString &destination, const QStringList &selection,
                    const QString &passphrase, const Progress &progress, qint64 totalHint = 0);
void addToArchive(const QString &path, const QStringList &sources, const QString &prefix,
                  const QString &passphrase, const Progress &progress);
void removeFromArchive(const QString &path, const QStringList &selection,
                       const QString &passphrase, const Progress &progress, qint64 totalHint = 0);

bool canModify(const QString &path);
bool looksLikeArchive(const QString &path);
QStringList creatableSuffixes();
QString normalizedEntryPath(QString path);

}
