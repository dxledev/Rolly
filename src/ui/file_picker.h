#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <functional>

namespace rolly {

// Asks the desktop's configured file chooser (xdg-desktop-portal FileChooser).
// Falls back to Qt's own dialog only when no portal is reachable.
class FilePicker : public QObject {
    Q_OBJECT
public:
    enum class Kind { OpenArchive, AddFiles, AddFolder, ChooseFolder, SaveArchive };
    using Done = std::function<void(const QStringList &paths)>;

    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    void pick(Kind kind, const QString &title, const QString &startFolder, const QString &suggestedName, Done done);
    bool busy() const { return bool(done_); }

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    Done done_;
    QString requestPath_;
    bool askPortal(Kind kind, const QString &title, const QString &startFolder, const QString &suggestedName);
    void askQt(Kind kind, const QString &title, const QString &startFolder, const QString &suggestedName);
    void finish(const QStringList &paths);
};

}
