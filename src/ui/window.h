#pragma once

#include "archive_model.h"
#include "core/archive.h"
#include "core/theme.h"
#include "file_picker.h"
#include "job.h"
#include "modal.h"
#include "theme_watcher.h"

#include <QWidget>
#include <functional>
#include <memory>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTableView;
class QTemporaryDir;

namespace rolly {

class EmptyState;

class Window : public QWidget {
    Q_OBJECT
public:
    Window(QString configDirectory, Theme theme, QWidget *parent = nullptr);
    ~Window() override;

    void openArchive(const QString &path);
    const QString &archivePath() const { return archivePath_; }
    ArchiveModel *model() { return &model_; }

protected:
    void closeEvent(QCloseEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;

private:
    using MakeWork = std::function<Job::Work(const QString &passphrase)>;

    QString configDirectory_, archivePath_, passphrase_;
    Listing listing_;
    ArchiveModel model_;
    QWidget *content_;
    QStackedWidget *stack_;
    EmptyState *empty_;
    QTableView *table_;
    QLabel *path_, *status_;
    QPushButton *up_, *openButton_, *newButton_, *extractButton_, *addButton_, *deleteButton_, *infoButton_;
    Modal *modal_;
    FilePicker *picker_;
    ThemeWatcher *watcher_;
    Job *job_ = nullptr;
    std::function<void(int)> onChosen_;
    std::function<void()> retry_;
    std::vector<std::unique_ptr<QTemporaryDir>> openedFiles_;

    void buildContent();
    void connectActions();
    void applyTheme();
    void updateActions();
    void refreshPath();
    QStringList selectedPaths() const;
    bool busy() const;

    void runJob(const QString &title, MakeWork make, std::function<void()> success = {});
    void askPassphrase(std::function<void()> retry);
    void showError(const QString &message);
    void ask(const QString &title, const QString &body, const QStringList &actions, std::function<void(int)> chosen, int focus = 0, bool destructive = false);
    bool translucent() const;

    void load(const QString &path);
    void chooseArchive();
    void createArchive();
    void createArchiveFrom(const QStringList &sources);
    void addEntries();
    void addSources(const QStringList &sources);
    void removeSelected();
    void extractSelected();
    void showInfo();
    void activateRow(const QModelIndex &index);
    void goUp();
    void chooseSources(bool folder, std::function<void(const QStringList &)> then);
};

}
