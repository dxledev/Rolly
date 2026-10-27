#include "file_picker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QUuid>

namespace rolly {
namespace {

constexpr auto service = "org.freedesktop.portal.Desktop";
constexpr auto requestInterface = "org.freedesktop.portal.Request";

}

void FilePicker::pick(Kind kind, const QString &title, const QString &startFolder, const QString &suggestedName, Done done) {
    if (busy())
        return;
    done_ = std::move(done);
    if (!askPortal(kind, title, startFolder, suggestedName))
        askQt(kind, title, startFolder, suggestedName);
}

void FilePicker::finish(const QStringList &paths) {
    auto done = std::move(done_);
    done_ = nullptr;
    if (!requestPath_.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(service, requestPath_, requestInterface, "Response",
                                                 this, SLOT(onResponse(uint, QVariantMap)));
        requestPath_.clear();
    }
    if (done)
        done(paths);
}

bool FilePicker::askPortal(Kind kind, const QString &title, const QString &startFolder, const QString &suggestedName) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;

    const QString token = "rolly" + QUuid::createUuid().toString(QUuid::Id128).left(12);
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    requestPath_ = "/org/freedesktop/portal/desktop/request/" + sender + '/' + token;

    QVariantMap options{{"handle_token", token}, {"modal", true}};
    if (kind == Kind::AddFiles)
        options.insert("multiple", true);
    if (kind == Kind::AddFolder || kind == Kind::ChooseFolder)
        options.insert("directory", true);
    if (kind == Kind::SaveArchive)
        options.insert("current_name", suggestedName);
    if (QFileInfo(startFolder).isDir())
        options.insert("current_folder", QFile::encodeName(startFolder) + '\0');

    // Subscribe first so a fast answer cannot slip past.
    if (!bus.connect(service, requestPath_, requestInterface, "Response", this, SLOT(onResponse(uint, QVariantMap)))) {
        requestPath_.clear();
        return false;
    }
    auto message = QDBusMessage::createMethodCall(service, "/org/freedesktop/portal/desktop", "org.freedesktop.portal.FileChooser",
                                                  kind == Kind::SaveArchive ? "SaveFile" : "OpenFile");
    message << QString() << title << options;

    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [=, this](QDBusPendingCallWatcher *call) {
        QDBusPendingReply<QDBusObjectPath> reply = *call;
        call->deleteLater();
        if (!reply.isError() || !busy())
            return;
        // No usable portal: undo the subscription and use Qt's dialog.
        QDBusConnection::sessionBus().disconnect(service, requestPath_, requestInterface, "Response",
                                                 this, SLOT(onResponse(uint, QVariantMap)));
        requestPath_.clear();
        askQt(kind, title, startFolder, suggestedName);
    });
    return true;
}

void FilePicker::onResponse(uint response, const QVariantMap &results) {
    QStringList paths;
    if (response == 0) {
        for (const auto &uri : qdbus_cast<QStringList>(results.value("uris"))) {
            const QUrl url(uri);
            if (url.isLocalFile())
                paths.append(url.toLocalFile());
        }
    }
    finish(paths);
}

void FilePicker::askQt(Kind kind, const QString &title, const QString &startFolder, const QString &suggestedName) {
    QStringList paths;
    switch (kind) {
    case Kind::OpenArchive:
        if (auto path = QFileDialog::getOpenFileName(nullptr, title, startFolder); !path.isEmpty())
            paths << path;
        break;
    case Kind::AddFiles:
        paths = QFileDialog::getOpenFileNames(nullptr, title, startFolder);
        break;
    case Kind::AddFolder:
    case Kind::ChooseFolder:
        if (auto path = QFileDialog::getExistingDirectory(nullptr, title, startFolder); !path.isEmpty())
            paths << path;
        break;
    case Kind::SaveArchive:
        if (auto path = QFileDialog::getSaveFileName(nullptr, title, QDir(startFolder).filePath(suggestedName)); !path.isEmpty())
            paths << path;
        break;
    }
    finish(paths);
}

}
