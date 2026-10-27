#include "archive_model.h"

#include "icons.h"

#include <QCollator>
#include <QFileInfo>
#include <QLocale>
#include <algorithm>

namespace rolly {

QString humanSize(qint64 bytes) {
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

int ArchiveModel::ensureNode(const QString &path, bool directory) {
    if (const auto found = byPath_.constFind(path); found != byPath_.constEnd()) {
        if (directory)
            nodes_[*found].directory = true;
        return *found;
    }
    const int slash = path.lastIndexOf('/');
    const int parent = slash < 0 ? 0 : ensureNode(path.left(slash), true);
    Node node;
    node.path = path;
    node.name = path.mid(slash + 1);
    node.directory = directory;
    nodes_.append(node);
    const int index = int(nodes_.size()) - 1;
    byPath_.insert(path, index);
    nodes_[parent].children.append(index);
    return index;
}

void ArchiveModel::setListing(const Listing &listing) {
    beginResetModel();
    nodes_.clear();
    byPath_.clear();
    nodes_.append(Node{});
    nodes_[0].directory = true;
    byPath_.insert(QString(), 0);
    totalSize_ = 0;
    entryCount_ = 0;
    for (const auto &entry : listing.entries) {
        const int index = ensureNode(entry.path, entry.directory);
        auto &node = nodes_[index];
        node.size = entry.size;
        node.modified = entry.modified;
        node.symlink = entry.symlink;
        node.encrypted = entry.encrypted;
        node.directory = node.directory || entry.directory;
        if (!entry.directory) {
            totalSize_ += entry.size;
            ++entryCount_;
        }
    }
    if (!byPath_.contains(directory_))
        directory_.clear();
    sortRows();
    endResetModel();
}

void ArchiveModel::clear() {
    setListing({});
    directory_.clear();
}

void ArchiveModel::sortRows() {
    const auto &children = nodes_.value(byPath_.value(directory_, 0)).children;
    rows_ = children;
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(rows_.begin(), rows_.end(), [&](int a, int b) {
        if (nodes_[a].directory != nodes_[b].directory)
            return nodes_[a].directory;
        return collator.compare(nodes_[a].name, nodes_[b].name) < 0;
    });
}

bool ArchiveModel::setDirectory(const QString &path) {
    if (!byPath_.contains(path) || !nodes_[byPath_[path]].directory)
        return false;
    beginResetModel();
    directory_ = path;
    sortRows();
    endResetModel();
    return true;
}

QString ArchiveModel::parentDirectory() const {
    const int slash = directory_.lastIndexOf('/');
    return slash < 0 ? QString() : directory_.left(slash);
}

QString ArchiveModel::pathAt(int row) const {
    return rows_.value(row, -1) < 0 ? QString() : nodes_[rows_[row]].path;
}

bool ArchiveModel::isDirectoryAt(int row) const {
    return rows_.value(row, -1) >= 0 && nodes_[rows_[row]].directory;
}

qint64 ArchiveModel::sizeAt(int row) const {
    return rows_.value(row, -1) < 0 ? 0 : sizeUnder(nodes_[rows_[row]].path);
}

qint64 ArchiveModel::sizeUnder(const QString &path) const {
    const auto found = byPath_.constFind(path);
    if (found == byPath_.constEnd())
        return 0;
    const auto &node = nodes_[*found];
    qint64 total = node.directory ? 0 : node.size;
    for (const int child : node.children)
        total += sizeUnder(nodes_[child].path);
    return total;
}

void ArchiveModel::setTheme(const Theme &theme) {
    folderIcon_ = glyphIcon(Glyph::Folder, theme.color("accent"), 18);
    fileIcon_ = glyphIcon(Glyph::File, theme.color("muted"), 18);
    linkIcon_ = glyphIcon(Glyph::Link, theme.color("muted"), 18);
    lockIcon_ = glyphIcon(Glyph::Lock, theme.color("muted"), 18);
    if (!rows_.isEmpty())
        emit dataChanged(index(0, 0), index(int(rows_.size()) - 1, ColumnCount - 1));
}

int ArchiveModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(rows_.size());
}

int ArchiveModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ArchiveModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const auto &node = nodes_[rows_[index.row()]];
    if (role == PathRole)
        return node.path;
    if (role == DirectoryRole)
        return node.directory;
    if (role == SizeRole)
        return node.directory ? 0 : node.size;
    if (role == Qt::TextAlignmentRole && index.column() == Size)
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::DecorationRole && index.column() == Name) {
        if (node.directory)
            return folderIcon_;
        if (node.encrypted)
            return lockIcon_;
        return node.symlink ? linkIcon_ : fileIcon_;
    }
    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
    case Name: return node.name;
    case Size: return node.directory ? QString() : humanSize(node.size);
    case Kind: {
        if (node.directory)
            return tr("Folder");
        const auto suffix = QFileInfo(node.name).suffix().toUpper();
        return suffix.isEmpty() ? tr("File") : suffix;
    }
    case Modified: return node.modified.isValid() ? QLocale().toString(node.modified, "yyyy-MM-dd  HH:mm") : QString();
    }
    return {};
}

QVariant ArchiveModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::TextAlignmentRole && section == Size)
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role != Qt::DisplayRole)
        return {};
    static const char *const titles[]{"name", "size", "kind", "modified"};
    return tr(titles[section]);
}

}
