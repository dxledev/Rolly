#include "job.h"

#include <QElapsedTimer>
#include <QThread>

namespace rolly {

Job::Job(Work work, QObject *parent) : QObject(parent), work_(std::move(work)) {}

Job::~Job() {
    cancelled_ = true;
    if (thread_) {
        thread_->wait();
        delete thread_;
    }
}

void Job::start() {
    thread_ = QThread::create([this] {
        QElapsedTimer clock;
        clock.start();
        QString error;
        bool needsPassphrase = false, cancelled = false;
        try {
            work_([&](qint64 done, qint64 total, const QString &current) {
                if (clock.elapsed() >= 40) {
                    clock.restart();
                    emit progress(done, total, current);
                }
                return !cancelled_.load();
            });
        } catch (const NeedsPassphrase &) {
            needsPassphrase = true;
        } catch (const Cancelled &) {
            cancelled = true;
        } catch (const std::exception &failure) {
            error = QString::fromUtf8(failure.what());
        }
        emit finished(error, needsPassphrase, cancelled);
    });
    connect(thread_, &QThread::finished, this, &QObject::deleteLater);
    thread_->start();
}

}
