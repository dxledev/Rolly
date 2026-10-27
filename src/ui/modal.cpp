#include "modal.h"

#include "modal_motion.h"

#include <QApplication>
#include <QDateTime>
#include <QTimer>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>

namespace rolly {

class ProgressBar : public QWidget {
public:
    explicit ProgressBar(QWidget *parent) : QWidget(parent) {
        setFixedHeight(6);
        pulse_.setInterval(33);
        connect(&pulse_, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    }
    void setTheme(const QColor &track, const QColor &fill) { track_ = track; fill_ = fill; update(); }
    void setValue(qint64 done, qint64 total) {
        value_ = total > 0 ? std::clamp(double(done) / double(total), 0.0, 1.0) : -1;
        if (value_ < 0 && isVisible())
            pulse_.start();
        else
            pulse_.stop();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(track_);
        p.drawRoundedRect(rect(), 3, 3);
        p.setBrush(fill_);
        if (value_ >= 0) {
            p.drawRoundedRect(QRectF(0, 0, std::max(6.0, width() * value_), height()), 3, 3);
        } else {
            // Unknown total: a short block that drifts with the clock.
            const qreal span = width() * .28;
            const qreal phase = (QDateTime::currentMSecsSinceEpoch() % 1400) / 1400.0;
            p.drawRoundedRect(QRectF((width() - span) * phase, 0, span, height()), 3, 3);
        }
    }

    void showEvent(QShowEvent *) override { if (value_ < 0) pulse_.start(); }
    void hideEvent(QHideEvent *) override { pulse_.stop(); }

private:
    QColor track_, fill_;
    QTimer pulse_;
    double value_ = 0;
};

Modal::Modal(QWidget *parent, QWidget *background)
    : QWidget(parent), background_(background), theme_(presetTheme("lofi")) {
    setObjectName("modal");
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoSystemBackground);

    card_ = new QWidget(this);
    card_->setObjectName("modalCard");
    motion_ = new ModalMotion(card_);
    card_->setGraphicsEffect(motion_);
    connect(motion_, &ModalMotion::progressChanged, this, [this] { update(); });
    connect(motion_, &ModalMotion::finished, this, [this](bool open) {
        if (!open && closing_)
            finishDismiss();
    });

    auto *layout = new QVBoxLayout(card_);
    layout->setContentsMargins(28, 26, 28, 24);
    layout->setSpacing(14);
    title_ = new QLabel(card_);
    title_->setObjectName("modalTitle");
    title_->setWordWrap(true);
    layout->addWidget(title_);
    body_ = new QLabel(card_);
    body_->setObjectName("modalBody");
    body_->setWordWrap(true);
    body_->setTextFormat(Qt::PlainText);
    layout->addWidget(body_);
    input_ = new QLineEdit(card_);
    input_->setEchoMode(QLineEdit::Password);
    input_->hide();
    input_->setContextMenuPolicy(Qt::NoContextMenu);
    layout->addWidget(input_);
    bar_ = new ProgressBar(card_);
    bar_->hide();
    layout->addWidget(bar_);
    detail_ = new QLabel(card_);
    detail_->setObjectName("modalDetail");
    detail_->setTextFormat(Qt::PlainText);
    detail_->hide();
    layout->addWidget(detail_);
    layout->addSpacing(4);

    for (int i = 0; i < 3; ++i) {
        auto *button = new QPushButton(card_);
        button->setObjectName(QString("modalAction%1").arg(i));
        button->setCursor(Qt::PointingHandCursor);
        layout->addWidget(button);
        buttons_.append(button);
        connect(button, &QPushButton::clicked, this, [this, i] {
            if (progressMode_)
                emit cancelRequested();
            else
                beginDismiss(i);
        });
    }
    connect(input_, &QLineEdit::returnPressed, this, [this] { beginDismiss(0); });
    for (auto *child : card_->findChildren<QWidget *>())
        child->installEventFilter(this);
    card_->installEventFilter(this);
    hide();
}

void Modal::setTheme(const Theme &theme) {
    theme_ = theme;
    bar_->setTheme(theme.color("surface_alt"), theme.color("accent"));
    if (isVisible())
        refreshSnapshot();
    update();
}

QString Modal::text() const {
    return input_->text();
}

void Modal::begin(const QString &title, const QString &body, const QStringList &actions) {
    if (!isVisible()) {
        previousFocus_ = QApplication::focusWidget();
        backgroundWasEnabled_ = background_ && background_->isEnabled();
        backgroundUpdatesWereEnabled_ = background_ && background_->updatesEnabled();
        motion_->reset();
    }
    closing_ = false;
    pendingChoice_ = -1;
    dismissible_ = true;
    progressMode_ = false;
    card_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    buttons_.first()->setProperty("danger", false);
    input_->hide();
    input_->clear();
    bar_->hide();
    detail_->hide();
    title_->setText(title);
    body_->setText(body);
    body_->setVisible(!body.isEmpty());
    for (int i = 0; i < buttons_.size(); ++i) {
        buttons_[i]->setVisible(i < actions.size());
        if (i < actions.size())
            buttons_[i]->setText(actions[i]);
    }
    setGeometry(parentWidget()->rect());
    centerCard();
    refreshSnapshot();
    if (background_)
        background_->setEnabled(false);
    freezeBackground();
    show();
    raise();
}

void Modal::open(const QString &title, const QString &body, const QStringList &actions, int focus, bool destructive) {
    begin(title, body, actions);
    if (destructive) {
        buttons_.first()->setProperty("danger", true);
        buttons_.first()->style()->unpolish(buttons_.first());
        buttons_.first()->style()->polish(buttons_.first());
    }
    if (focus >= 0 && focus < actions.size())
        buttons_[focus]->setFocus();
    motion_->animate(true);
}

void Modal::openPassword(const QString &title, const QString &body, const QStringList &actions) {
    begin(title, body, actions);
    input_->show();
    centerCard();
    input_->setFocus();
    motion_->animate(true);
}

void Modal::openProgress(const QString &title, const QString &cancelText) {
    begin(title, {}, {cancelText});
    dismissible_ = false;
    progressMode_ = true;
    bar_->setValue(0, 0);
    bar_->show();
    detail_->setText({});
    detail_->show();
    centerCard();
    buttons_.first()->setFocus();
    motion_->animate(true);
}

void Modal::setProgress(qint64 done, qint64 total, const QString &current) {
    if (!progressMode_)
        return;
    bar_->setValue(done, total);
    detail_->setText(detail_->fontMetrics().elidedText(current, Qt::ElideMiddle, card_->width() - 56));
}

void Modal::centerCard() {
    card_->setFixedWidth(std::min(460, std::max(200, width() - 40)));
    card_->layout()->activate();
    card_->adjustSize();
    card_->move((width() - card_->width()) / 2, (height() - card_->height()) / 2);
}

void Modal::refreshSnapshot() {
    if (!background_ || background_->size().isEmpty())
        return;
    if (background_->layout())
        background_->layout()->activate();
    if (backgroundFrozen_ && backgroundUpdatesWereEnabled_)
        background_->setUpdatesEnabled(true);
    auto captured = background_->grab();
    if (backgroundFrozen_)
        background_->setUpdatesEnabled(false);
    backgroundFrame_ = captured;
    if (captured.isNull())
        return;
    QSize small = (captured.deviceIndependentSize().toSize() / 3).expandedTo(QSize(1, 1));
    if (small.width() > 640 || small.height() > 640)
        small.scale(640, 640, Qt::KeepAspectRatio);
    auto source = captured.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                      .toImage()
                      .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (source.isNull() || source.width() < 1 || source.height() < 1)
        return;
    // Repeat the border pixels so the blur does not fade out at the window edge.
    constexpr int padding = 32;
    QImage extended(source.size() + QSize(padding * 2, padding * 2), QImage::Format_ARGB32_Premultiplied);
    if (extended.isNull())
        return;
    for (int y = 0; y < extended.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(source.constScanLine(std::clamp(y - padding, 0, source.height() - 1)));
        auto *out = reinterpret_cast<QRgb *>(extended.scanLine(y));
        std::fill_n(out, padding, row[0]);
        std::copy_n(row, source.width(), out + padding);
        std::fill_n(out + padding + source.width(), padding, row[source.width() - 1]);
    }
    QGraphicsScene scene;
    auto *item = scene.addPixmap(QPixmap::fromImage(extended));
    auto *blur = new QGraphicsBlurEffect;
    blur->setBlurRadius(5);
    blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
    item->setGraphicsEffect(blur);
    QPixmap result(small);
    result.fill(Qt::transparent);
    QPainter p(&result);
    scene.render(&p, QRectF(QPointF(), small), QRectF(QPointF(padding, padding), small));
    p.end();
    composeSnapshot(result);
    update();
}

void Modal::composeSnapshot(const QPixmap &blurred) {
    const auto ratio = devicePixelRatioF();
    snapshot_ = QPixmap(QSize(qCeil(width() * ratio), qCeil(height() * ratio)));
    snapshot_.setDevicePixelRatio(ratio);
    snapshot_.fill(Qt::transparent);
    QPainter p(&snapshot_);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawPixmap(rect(), blurred);
    auto tint = theme_.color("background");
    tint.setAlphaF(std::max(.7, double(tint.alphaF())));
    p.fillRect(rect(), tint);
}

void Modal::freezeBackground() {
    if (!background_ || backgroundFrozen_)
        return;
    background_->setUpdatesEnabled(false);
    backgroundFrozen_ = true;
}

void Modal::restoreBackgroundUpdates() {
    if (background_ && backgroundFrozen_)
        background_->setUpdatesEnabled(backgroundUpdatesWereEnabled_);
    backgroundFrozen_ = false;
}

void Modal::dismiss() {
    beginDismiss(-1);
}

void Modal::beginDismiss(int choice) {
    if (!isVisible() || closing_)
        return;
    closing_ = true;
    pendingChoice_ = choice;
    card_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    motion_->animate(false);
}

void Modal::finishDismiss() {
    const int choice = pendingChoice_;
    pendingChoice_ = -1;
    closing_ = false;
    hide();
    if (background_)
        background_->setEnabled(backgroundWasEnabled_);
    restoreBackgroundUpdates();
    if (previousFocus_ && previousFocus_->isEnabled() && previousFocus_->isVisible())
        previousFocus_->setFocus();
    previousFocus_.clear();
    if (choice >= 0)
        emit chosen(choice);
    emit dismissed();
}

bool Modal::eventFilter(QObject *object, QEvent *event) {
    if (closing_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease
                     || event->type() == QEvent::Wheel || event->type() == QEvent::ShortcutOverride))
        return true;
    return QWidget::eventFilter(object, event);
}

bool Modal::event(QEvent *event) {
    const bool handled = QWidget::event(event);
    if (event->type() == QEvent::DevicePixelRatioChange && isVisible())
        refreshSnapshot();
    return handled;
}

void Modal::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawPixmap(QPointF(), backgroundFrame_);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setOpacity(motion_->progress());
    p.drawPixmap(QPointF(), snapshot_);
}

void Modal::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    centerCard();
    if (isVisible())
        refreshSnapshot();
}

void Modal::hideEvent(QHideEvent *event) {
    snapshot_ = QPixmap();
    backgroundFrame_ = QPixmap();
    restoreBackgroundUpdates();
    QWidget::hideEvent(event);
}

void Modal::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (progressMode_)
            emit cancelRequested();
        else if (dismissible_)
            dismiss();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void Modal::mousePressEvent(QMouseEvent *event) {
    if (dismissible_ && !closing_ && !card_->geometry().contains(event->position().toPoint()))
        dismiss();
    event->accept();
}

}
