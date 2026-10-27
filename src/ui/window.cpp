#include "window.h"

#include "icons.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QWindow>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QTableView>
#include <QTemporaryDir>
#include <QUrl>
#include <QVBoxLayout>

namespace rolly {
namespace {

QString css(QColor color) {
    return QString("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}

QPushButton *makeButton(const QString &text, const QString &name, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setObjectName(name);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QIcon twoStateIcon(Glyph glyph, const QColor &normal, const QColor &disabled) {
    QIcon icon;
    for (const qreal ratio : {1.0, 2.0}) {
        icon.addPixmap(glyphPixmap(glyph, normal, 18, ratio), QIcon::Normal);
        icon.addPixmap(glyphPixmap(glyph, disabled, 18, ratio), QIcon::Disabled);
    }
    return icon;
}

QString plural(qsizetype count, const QString &singular) {
    return QString("%1 %2%3").arg(count).arg(singular).arg(count == 1 ? "" : "s");
}

}

class EmptyState : public QWidget {
public:
    explicit EmptyState(QWidget *parent) : QWidget(parent) { setObjectName("empty"); }
    void setTheme(const Theme &theme) { theme_ = theme; update(); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPen border(theme_.color("border"), 1.5, Qt::CustomDashLine);
        border.setDashPattern({4, 4});
        p.setPen(border);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 14, 14);
        const int cx = width() / 2, cy = height() / 2;
        p.drawPixmap(cx - 28, cy - 74, glyphPixmap(Glyph::Archive, theme_.color("accent"), 56, devicePixelRatioF()));
        QFont title = font();
        title.setPointSizeF(font().pointSizeF() * 1.35);
        title.setWeight(QFont::DemiBold);
        p.setFont(title);
        p.setPen(theme_.color("text"));
        p.drawText(QRect(0, cy - 4, width(), 30), Qt::AlignCenter, tr("drop an archive here"));
        QFont hint = font();
        hint.setFamily("monospace");
        hint.setPointSizeF(font().pointSizeF() * .92);
        p.setFont(hint);
        p.setPen(theme_.color("muted"));
        p.drawText(QRect(0, cy + 28, width(), 24), Qt::AlignCenter, tr("or press Ctrl+O to open one"));
    }

private:
    Theme theme_ = presetTheme("lofi");
};

Window::Window(QString directory, Theme theme, QWidget *parent) : QWidget(parent), configDirectory_(std::move(directory)) {
    setWindowTitle("Rolly");
    setObjectName("rollyWindow");
    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);
    setMinimumSize(640, 440);
    resize(960, 640);
    setFont(QFont("Noto Sans", 10));

    buildContent();
    modal_ = new Modal(this, content_);
    picker_ = new FilePicker(this);
    watcher_ = new ThemeWatcher(configDirectory_ + "/theme.json", std::move(theme), this);
    connectActions();
    applyTheme();
    updateActions();
    refreshPath();
}

Window::~Window() {
    if (job_)
        job_->cancel();
}

void Window::buildContent() {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    content_ = new QWidget(this);
    content_->setObjectName("content");
    outer->addWidget(content_);

    auto *layout = new QVBoxLayout(content_);
    layout->setContentsMargins(20, 16, 20, 12);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    header->setSpacing(4);
    auto *brand = new QLabel("Rolly", content_);
    brand->setObjectName("brand");
    header->addWidget(brand);
    header->addSpacing(14);
    openButton_ = makeButton(tr("Open"), "open", content_);
    newButton_ = makeButton(tr("New"), "new", content_);
    header->addWidget(openButton_);
    header->addWidget(newButton_);
    header->addStretch();
    addButton_ = makeButton(tr("Add"), "add", content_);
    deleteButton_ = makeButton(tr("Delete"), "delete", content_);
    infoButton_ = makeButton(tr("Info"), "info", content_);
    extractButton_ = makeButton(tr("Extract"), "primary", content_);
    header->addWidget(addButton_);
    header->addWidget(deleteButton_);
    header->addWidget(infoButton_);
    header->addSpacing(6);
    header->addWidget(extractButton_);
    layout->addLayout(header);

    auto *bar = new QHBoxLayout;
    bar->setSpacing(6);
    up_ = makeButton({}, "up", content_);
    up_->setFixedSize(32, 32);
    path_ = new QLabel(content_);
    path_->setObjectName("path");
    path_->setTextInteractionFlags(Qt::NoTextInteraction);
    bar->addWidget(up_);
    bar->addWidget(path_, 1);
    layout->addLayout(bar);

    stack_ = new QStackedWidget(content_);
    empty_ = new EmptyState(stack_);
    table_ = new QTableView(stack_);
    table_->setModel(&model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setIconSize(QSize(18, 18));
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideMiddle);
    table_->setFocusPolicy(Qt::StrongFocus);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(32);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->horizontalHeader()->setSectionsClickable(false);
    table_->horizontalHeader()->setSectionResizeMode(ArchiveModel::Name, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(ArchiveModel::Size, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(ArchiveModel::Kind, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(ArchiveModel::Modified, QHeaderView::ResizeToContents);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->verticalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);
    table_->setContextMenuPolicy(Qt::NoContextMenu);
    stack_->addWidget(empty_);
    stack_->addWidget(table_);
    layout->addWidget(stack_, 1);

    status_ = new QLabel(content_);
    status_->setObjectName("status");
    status_->setWordWrap(true);
    layout->addWidget(status_);
}

void Window::connectActions() {
    connect(openButton_, &QPushButton::clicked, this, &Window::chooseArchive);
    connect(newButton_, &QPushButton::clicked, this, &Window::createArchive);
    connect(addButton_, &QPushButton::clicked, this, &Window::addEntries);
    connect(deleteButton_, &QPushButton::clicked, this, &Window::removeSelected);
    connect(infoButton_, &QPushButton::clicked, this, &Window::showInfo);
    connect(extractButton_, &QPushButton::clicked, this, &Window::extractSelected);
    connect(up_, &QPushButton::clicked, this, &Window::goUp);
    connect(table_, &QTableView::activated, this, &Window::activateRow);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); });
    connect(&model_, &QAbstractItemModel::modelReset, this, [this] { updateActions(); refreshPath(); });

    connect(watcher_, &ThemeWatcher::changed, this, &Window::applyTheme);
    connect(watcher_, &ThemeWatcher::rejected, this, [this](const QString &error) {
        status_->setText(tr("theme unchanged: %1").arg(error));
    });

    connect(modal_, &Modal::chosen, this, [this](int index) {
        if (auto handler = std::exchange(onChosen_, nullptr))
            handler(index);
    });
    connect(modal_, &Modal::dismissed, this, [this] { onChosen_ = nullptr; });
    connect(modal_, &Modal::cancelRequested, this, [this] {
        if (job_)
            job_->cancel();
    });

    auto shortcut = [this](const QKeySequence &keys, auto action) {
        auto *key = new QShortcut(keys, this);
        connect(key, &QShortcut::activated, this, [this, action] {
            if (!modal_->isVisible() && !busy())
                action();
        });
    };
    shortcut(QKeySequence("Ctrl+O"), [this] { chooseArchive(); });
    shortcut(QKeySequence("Ctrl+N"), [this] { createArchive(); });
    shortcut(QKeySequence("Ctrl+E"), [this] { extractSelected(); });
    shortcut(QKeySequence("Ctrl+Shift+A"), [this] { addEntries(); });
    shortcut(QKeySequence("Delete"), [this] { removeSelected(); });
    shortcut(QKeySequence("Alt+Return"), [this] { showInfo(); });
    shortcut(QKeySequence("Alt+Up"), [this] { goUp(); });
    shortcut(QKeySequence("Backspace"), [this] { goUp(); });
    shortcut(QKeySequence("Ctrl+W"), [this] {
        archivePath_.clear();
        listing_ = {};
        passphrase_.clear();
        model_.clear();
        stack_->setCurrentWidget(empty_);
        setWindowTitle("Rolly");
        updateActions();
        refreshPath();
    });
    shortcut(QKeySequence("Ctrl+Q"), [this] { close(); });
}

bool Window::translucent() const {
    // Wayland compositors always blend; elsewhere trust the surface the window actually got.
    const auto platform = QGuiApplication::platformName();
    if (platform.startsWith("wayland") || platform == "offscreen" || platform == "minimal")
        return true;
    return windowHandle() && windowHandle()->format().hasAlpha();
}

void Window::applyTheme() {
    // The saved palette keeps its alpha; only what we draw is flattened when blending is unavailable.
    const Theme theme = translucent() ? watcher_->theme() : opaqueTheme(watcher_->theme());
    const QMap<QString, QString> tokens{
        {"@bg@", css(theme.color("background"))}, {"@surface@", css(theme.color("surface"))},
        {"@alt@", css(theme.color("surface_alt"))}, {"@text@", css(theme.color("text"))},
        {"@muted@", css(theme.color("muted"))}, {"@accent@", css(theme.color("accent"))},
        {"@accentText@", css(theme.color("accent_text"))}, {"@border@", css(theme.color("border"))},
        {"@selection@", css(theme.color("selection"))}, {"@danger@", css(theme.color("danger"))},
    };
    QString sheet = R"(
QWidget#rollyWindow {background:transparent;}
QWidget#content {background:@bg@;}
QWidget {color:@text@;font-family:'Noto Sans';font-size:13px;}
QLabel {background:transparent;}
QLabel#brand {font-size:19px;font-weight:600;color:@accent@;}
QLabel#path {font-family:monospace;font-size:12px;color:@muted@;}
QLabel#status {font-family:monospace;font-size:11px;color:@muted@;}
QPushButton {background:transparent;border:1px solid transparent;border-radius:6px;padding:7px 12px;}
QPushButton:hover {background:@alt@;}
QPushButton:focus {border-color:@accent@;}
QPushButton:disabled {color:@muted@;}
QPushButton#primary {background:@accent@;color:@accentText@;}
QPushButton#primary:disabled {background:transparent;color:@muted@;}
QPushButton#up {padding:0;}
QTableView {background:transparent;border:none;outline:0;selection-background-color:@selection@;selection-color:@text@;}
QTableView::item {padding:0 8px;border:none;}
QTableView::item:selected {background:@selection@;}
QTableView::item:hover {background:@alt@;}
QHeaderView {background:transparent;}
QHeaderView::section {background:transparent;color:@muted@;border:none;border-bottom:1px dashed @border@;padding:6px 8px;font-size:11px;}
QScrollBar:vertical {background:transparent;width:10px;margin:0;}
QScrollBar::handle:vertical {background:@border@;border-radius:4px;min-height:28px;margin:2px;}
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}
QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical {background:transparent;}
QLineEdit {background:@alt@;border:1px solid @border@;border-radius:6px;padding:8px 10px;selection-background-color:@accent@;selection-color:@accentText@;}
QLineEdit:focus {border-color:@accent@;}
QWidget#modalCard {background:@surface@;border:1px solid @border@;border-radius:12px;}
QWidget#modalCard QLabel {border:none;}
QLabel#modalTitle {font-size:20px;font-weight:600;}
QLabel#modalDetail {color:@muted@;font-family:monospace;font-size:11px;}
QWidget#modalCard QPushButton {background:@alt@;border:1px solid @border@;min-height:20px;}
QWidget#modalCard QPushButton:hover {border-color:@accent@;}
QWidget#modalCard QPushButton:focus {border-color:@accent@;}
QWidget#modalCard QPushButton#modalAction0 {background:@accent@;color:@accentText@;border-color:@accent@;}
QWidget#modalCard QPushButton#modalAction0[danger="true"] {background:@danger@;color:@accentText@;border-color:@danger@;}
)";
    for (auto token = tokens.cbegin(); token != tokens.cend(); ++token)
        sheet.replace(token.key(), token.value());
    setStyleSheet(sheet);

    const auto text = theme.color("text"), muted = theme.color("muted");
    openButton_->setIcon(twoStateIcon(Glyph::Open, text, muted));
    newButton_->setIcon(twoStateIcon(Glyph::New, text, muted));
    addButton_->setIcon(twoStateIcon(Glyph::Add, text, muted));
    deleteButton_->setIcon(twoStateIcon(Glyph::Remove, text, muted));
    infoButton_->setIcon(twoStateIcon(Glyph::Info, text, muted));
    extractButton_->setIcon(twoStateIcon(Glyph::Extract, theme.color("accent_text"), muted));
    up_->setIcon(twoStateIcon(Glyph::Up, text, muted));
    up_->setIconSize(QSize(18, 18));
    model_.setTheme(theme);
    empty_->setTheme(theme);
    modal_->setTheme(theme);
}

bool Window::busy() const {
    return job_ || picker_->busy();
}

void Window::updateActions() {
    const bool open = !archivePath_.isEmpty();
    const bool writable = open && canModify(archivePath_);
    const bool selected = table_->selectionModel() && table_->selectionModel()->hasSelection();
    extractButton_->setEnabled(open);
    extractButton_->setText(selected ? tr("Extract selected") : tr("Extract"));
    addButton_->setEnabled(writable);
    deleteButton_->setEnabled(writable && selected);
    infoButton_->setEnabled(open);
    up_->setEnabled(open && !model_.directory().isEmpty());
    if (!open) {
        status_->setText(tr("no archive open"));
        return;
    }
    status_->setText(QString("%1  ·  %2 unpacked  ·  %3%4")
                         .arg(plural(model_.entryCount(), "file"), humanSize(model_.totalSize()), listing_.format,
                              writable ? QString() : tr("  ·  read-only")));
}

void Window::refreshPath() {
    if (archivePath_.isEmpty()) {
        path_->setText({});
        return;
    }
    QString text = QFileInfo(archivePath_).fileName();
    if (!model_.directory().isEmpty())
        text += " / " + QString(model_.directory()).replace('/', " / ");
    path_->setText(path_->fontMetrics().elidedText(text, Qt::ElideMiddle, std::max(100, width() - 120)));
}

QStringList Window::selectedPaths() const {
    QStringList paths;
    for (const auto &index : table_->selectionModel()->selectedRows())
        paths.append(model_.pathAt(index.row()));
    return paths;
}

void Window::ask(const QString &title, const QString &body, const QStringList &actions, std::function<void(int)> chosen, int focus, bool destructive) {
    onChosen_ = std::move(chosen);
    modal_->open(title, body, actions, focus, destructive);
}

void Window::showError(const QString &message) {
    ask(tr("That did not work"), message, {tr("Close")}, nullptr);
}

void Window::askPassphrase(std::function<void()> retry) {
    onChosen_ = [this, retry = std::move(retry)](int index) {
        if (index != 0)
            return;
        passphrase_ = modal_->text();
        retry();
    };
    modal_->openPassword(tr("Password needed"), tr("This archive is protected. Enter its password."), {tr("Unlock"), tr("Cancel")});
}

void Window::runJob(const QString &title, MakeWork make, std::function<void()> success) {
    if (job_)
        return;
    modal_->openProgress(title, tr("Cancel"));
    job_ = new Job(make(passphrase_), this);
    connect(job_, &Job::progress, modal_, &Modal::setProgress);
    connect(job_, &Job::finished, this, [=, this](const QString &error, bool needsPassphrase, bool cancelled) {
        job_ = nullptr;
        if (needsPassphrase) {
            askPassphrase([=, this] { runJob(title, make, success); });
        } else if (cancelled) {
            modal_->dismiss();
        } else if (!error.isEmpty()) {
            showError(error);
        } else if (success) {
            success();
        } else {
            modal_->dismiss();
        }
    });
    job_->start();
}

void Window::load(const QString &path) {
    const auto listing = std::make_shared<Listing>();
    runJob(tr("Reading archive"), [path, listing](const QString &passphrase) -> Job::Work {
        return [=](const Progress &) { *listing = readArchive(path, passphrase); };
    }, [=, this] {
        const bool sameArchive = path == archivePath_;
        archivePath_ = path;
        listing_ = *listing;
        model_.setListing(listing_);
        if (!sameArchive)
            model_.setDirectory({});
        stack_->setCurrentWidget(table_);
        setWindowTitle(QFileInfo(path).fileName() + " — Rolly");
        updateActions();
        refreshPath();
        table_->setFocus();
        modal_->dismiss();
    });
}

void Window::openArchive(const QString &path) {
    if (busy() || modal_->isVisible())
        return;
    const QFileInfo info(path);
    if (!info.isFile()) {
        showError(tr("%1 is not a file.").arg(path));
        return;
    }
    if (info.absoluteFilePath() != archivePath_)
        passphrase_.clear();
    load(info.absoluteFilePath());
}

void Window::chooseArchive() {
    const QString start = archivePath_.isEmpty() ? QDir::homePath() : QFileInfo(archivePath_).absolutePath();
    picker_->pick(FilePicker::Kind::OpenArchive, tr("Open archive"), start, {}, [this](const QStringList &paths) {
        if (!paths.isEmpty())
            openArchive(paths.first());
    });
}

void Window::chooseSources(bool folder, std::function<void(const QStringList &)> then) {
    picker_->pick(folder ? FilePicker::Kind::AddFolder : FilePicker::Kind::AddFiles, folder ? tr("Choose a folder") : tr("Choose files"),
                  QDir::homePath(), {}, [then = std::move(then)](const QStringList &paths) {
                      if (!paths.isEmpty())
                          then(paths);
                  });
}

void Window::createArchive() {
    ask(tr("New archive"), tr("Choose what goes inside."), {tr("Files…"), tr("Folder…"), tr("Cancel")}, [this](int index) {
        if (index < 2)
            chooseSources(index == 1, [this](const QStringList &sources) { createArchiveFrom(sources); });
    });
}

void Window::createArchiveFrom(const QStringList &sources) {
    const QFileInfo first(sources.first());
    const QString base = sources.size() == 1 ? first.completeBaseName().isEmpty() ? first.fileName() : first.fileName() : QString("archive");
    picker_->pick(FilePicker::Kind::SaveArchive, tr("Save archive as (.zip .7z .tar.gz .tar.xz .tar.bz2 .tar.zst)"),
                  first.absolutePath(), base + ".zip", [this, sources](const QStringList &paths) {
        if (paths.isEmpty())
            return;
        QString destination = paths.first();
        if (!canModify(destination))
            destination += ".zip";
        QFile::remove(destination);
        passphrase_.clear();
        runJob(tr("Creating archive"), [=](const QString &passphrase) -> Job::Work {
            return [=](const Progress &progress) { addToArchive(destination, sources, {}, passphrase, progress); };
        }, [=, this] {
            modal_->dismiss();
            QTimer::singleShot(0, this, [=, this] { openArchive(destination); });
        });
    });
}

void Window::addEntries() {
    if (archivePath_.isEmpty() || !canModify(archivePath_))
        return;
    ask(tr("Add to archive"), tr("Choose what to add to %1.").arg(model_.directory().isEmpty() ? tr("the top level") : model_.directory()),
        {tr("Files…"), tr("Folder…"), tr("Cancel")}, [this](int index) {
            if (index < 2)
                chooseSources(index == 1, [this](const QStringList &sources) { addSources(sources); });
        });
}

void Window::addSources(const QStringList &sources) {
    const QString archivePath = archivePath_, prefix = model_.directory();
    runJob(tr("Adding files"), [=](const QString &passphrase) -> Job::Work {
        return [=](const Progress &progress) { addToArchive(archivePath, sources, prefix, passphrase, progress); };
    }, [=, this] { load(archivePath); });
}

void Window::removeSelected() {
    const auto paths = selectedPaths();
    if (archivePath_.isEmpty() || !canModify(archivePath_) || paths.isEmpty())
        return;
    ask(tr("Delete from archive?"), tr("%1 will be removed from the archive file. This cannot be undone.").arg(plural(paths.size(), "item")),
        {tr("Delete"), tr("Keep")}, [this, paths](int index) {
            if (index != 0)
                return;
            const QString archivePath = archivePath_;
            const qint64 total = model_.totalSize();
            runJob(tr("Removing"), [=](const QString &passphrase) -> Job::Work {
                return [=](const Progress &progress) { removeFromArchive(archivePath, paths, passphrase, progress, total); };
            }, [=, this] { load(archivePath); });
        }, 1, true);
}

void Window::extractSelected() {
    if (archivePath_.isEmpty())
        return;
    const auto paths = selectedPaths();
    const QString archivePath = archivePath_;
    qint64 total = model_.totalSize();
    if (!paths.isEmpty()) {
        total = 0;
        for (const auto &path : paths)
            total += model_.sizeUnder(path);
    }
    picker_->pick(FilePicker::Kind::ChooseFolder, tr("Extract to"), QFileInfo(archivePath).absolutePath(), {},
                  [=, this](const QStringList &folders) {
        if (folders.isEmpty())
            return;
        const QString destination = folders.first();
        auto start = [=, this] {
            runJob(tr("Extracting"), [=](const QString &passphrase) -> Job::Work {
                return [=](const Progress &progress) { extractArchive(archivePath, destination, paths, passphrase, progress, total); };
            }, [=, this] {
                ask(tr("Extracted"), tr("Files are in %1.").arg(destination), {tr("Show folder"), tr("Done")}, [destination](int index) {
                    if (index == 0)
                        QDesktopServices::openUrl(QUrl::fromLocalFile(destination));
                });
            });
        };
        // Extraction keeps entry paths, so these are the names it would write first.
        QStringList targets = paths;
        if (targets.isEmpty())
            for (const auto &entry : listing_.entries)
                if (!entry.path.contains('/'))
                    targets.append(entry.path);
        QStringList clashes;
        for (const auto &name : targets) {
            const QFileInfo existing(destination + '/' + name);
            if (existing.exists() || existing.isSymLink())
                clashes.append(name);
        }
        if (clashes.isEmpty()) {
            start();
            return;
        }
        ask(tr("Replace existing files?"),
            tr("%1 already exist in %2 and will be replaced.").arg(plural(clashes.size(), "item"), destination),
            {tr("Replace"), tr("Cancel")}, [start](int index) { if (index == 0) start(); }, 1, true);
    });
}

void Window::showInfo() {
    if (archivePath_.isEmpty())
        return;
    const QFileInfo info(archivePath_);
    const qint64 packed = info.size();
    const QString ratio = model_.totalSize() > 0 ? QString("%1%").arg(100 * packed / model_.totalSize()) : QStringLiteral("—");
    const QString body = QString("%1\n%2\n\nformat      %3\nfiles       %4\npacked      %5\nunpacked    %6\nratio       %7%8")
        .arg(info.fileName(), info.absolutePath(), listing_.format).arg(model_.entryCount()).arg(humanSize(packed), humanSize(model_.totalSize()), ratio,
             listing_.encrypted ? tr("\npassword    required") : QString());
    ask(tr("Archive details"), body, {tr("Close")}, nullptr);
}

void Window::activateRow(const QModelIndex &index) {
    if (!index.isValid())
        return;
    const QString path = model_.pathAt(index.row());
    if (model_.isDirectoryAt(index.row())) {
        model_.setDirectory(path);
        return;
    }
    // Unpack the single file somewhere temporary and let the desktop open it.
    auto folder = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/rolly-XXXXXX");
    if (!folder->isValid())
        return;
    const QString archivePath = archivePath_, destination = folder->path();
    const qint64 total = model_.sizeAt(index.row());
    openedFiles_.push_back(std::move(folder));
    runJob(tr("Opening file"), [=](const QString &passphrase) -> Job::Work {
        return [=](const Progress &progress) { extractArchive(archivePath, destination, {path}, passphrase, progress, total); };
    }, [=, this] {
        modal_->dismiss();
        QDesktopServices::openUrl(QUrl::fromLocalFile(destination + '/' + path));
    });
}

void Window::goUp() {
    if (!archivePath_.isEmpty() && !model_.directory().isEmpty())
        model_.setDirectory(model_.parentDirectory());
}

void Window::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls() && !modal_->isVisible())
        event->acceptProposedAction();
}

void Window::dropEvent(QDropEvent *event) {
    QStringList paths;
    for (const auto &url : event->mimeData()->urls())
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
    if (paths.isEmpty() || busy() || modal_->isVisible())
        return;
    event->acceptProposedAction();
    if (archivePath_.isEmpty() && paths.size() == 1 && looksLikeArchive(paths.first()))
        openArchive(paths.first());
    else if (archivePath_.isEmpty())
        QTimer::singleShot(0, this, [this, paths] { createArchiveFrom(paths); });
    else if (canModify(archivePath_))
        QTimer::singleShot(0, this, [this, paths] { addSources(paths); });
    else
        openArchive(paths.first());
}

void Window::closeEvent(QCloseEvent *event) {
    if (job_) {
        job_->cancel();
        job_->disconnect(this);
    }
    QWidget::closeEvent(event);
}

}
