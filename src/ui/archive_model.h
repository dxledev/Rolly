#pragma once

#include "core/archive.h"
#include "core/theme.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QIcon>

namespace rolly {

class ArchiveModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Name, Size, Kind, Modified, ColumnCount };
    enum Role { PathRole = Qt::UserRole + 1, DirectoryRole, SizeRole };

    explicit ArchiveModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    void setListing(const Listing &listing);
    void clear();
    void setTheme(const Theme &theme);
    bool setDirectory(const QString &path);
    const QString &directory() const { return directory_; }
    QString parentDirectory() const;
    QString pathAt(int row) const;
    bool isDirectoryAt(int row) const;
    qint64 sizeAt(int row) const;
    qint64 totalSize() const { return totalSize_; }
    qsizetype entryCount() const { return entryCount_; }
    qint64 sizeUnder(const QString &path) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    struct Node {
        QString name;
        QString path;
        qint64 size = 0;
        QDateTime modified;
        bool directory = false, symlink = false, encrypted = false;
        QVector<int> children;
    };
    QVector<Node> nodes_;
    QHash<QString, int> byPath_;
    QVector<int> rows_;
    QString directory_;
    qint64 totalSize_ = 0;
    qsizetype entryCount_ = 0;
    QIcon folderIcon_, fileIcon_, linkIcon_, lockIcon_;

    int ensureNode(const QString &path, bool directory);
    void sortRows();
};

QString humanSize(qint64 bytes);

}
