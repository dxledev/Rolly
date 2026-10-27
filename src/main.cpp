#include "cli.h"
#include "core/storage.h"
#include "core/theme.h"
#include "ui/window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QProcess>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication::setApplicationName("rolly");
    QCoreApplication::setApplicationVersion("1.0.0");

    QStringList args;
    for (int i = 1; i < argc; ++i)
        args.append(QString::fromLocal8Bit(argv[i]));
    try {
        bool command = false;
        for (const auto &arg : args)
            if (arg == "theme" || arg == "--help" || arg == "-h" || arg == "--version")
                command = true;
        if (command) {
            QCoreApplication app(argc, argv);
            return rolly::runCli(args);
        }
        QStringList files;
        const auto result = rolly::runCli(args, &files);
        if (result != -1)
            return result;
        const auto directory = rolly::configDirectory(args);

        if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "wayland");
        // The interface is raster-only; shared-memory buffers avoid a GPU stack.
        if (qEnvironmentVariableIsEmpty("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION"))
            qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "shm");
        QApplication app(argc, argv);
        app.setStyle("Fusion");
        app.setDesktopFileName("io.github.rolly");
        app.setWindowIcon(QIcon::fromTheme("io.github.rolly"));

        const auto theme = rolly::ensureTheme(directory + "/theme.json");
        // Each extra archive gets its own window process.
        for (qsizetype i = 1; i < files.size(); ++i)
            QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--config-dir", directory, files[i]});
        rolly::Window window(directory, theme);
        window.show();
        if (!files.isEmpty())
            window.openArchive(files.first());
        return app.exec();
    } catch (const std::exception &error) {
        QTextStream(stderr) << "rolly: " << error.what() << '\n';
        return 1;
    }
}
