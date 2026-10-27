#pragma once

#include "core/archive.h"

#include <QObject>
#include <atomic>
#include <functional>

class QThread;

namespace rolly {

// Runs one archive operation off the UI thread and reports progress.
class Job : public QObject {
    Q_OBJECT
public:
    using Work = std::function<void(const Progress &)>;
    explicit Job(Work work, QObject *parent = nullptr);
    ~Job() override;
    void start();
    void cancel() { cancelled_ = true; }

signals:
    void progress(qint64 done, qint64 total, const QString &current);
    void finished(const QString &error, bool needsPassphrase, bool cancelled);

private:
    Work work_;
    QThread *thread_ = nullptr;
    std::atomic_bool cancelled_{false};
};

}
