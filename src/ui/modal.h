#pragma once

#include "core/theme.h"

#include <QPixmap>
#include <QPointer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace rolly {

class ModalMotion;
class ProgressBar;

// A dialog drawn inside the window over a softly blurred copy of the content.
class Modal : public QWidget {
    Q_OBJECT
public:
    Modal(QWidget *parent, QWidget *background);

    void open(const QString &title, const QString &body, const QStringList &actions, int focus = 0, bool destructive = false);
    void openPassword(const QString &title, const QString &body, const QStringList &actions);
    void openProgress(const QString &title, const QString &cancelText);
    void setProgress(qint64 done, qint64 total, const QString &current);
    QString text() const;
    bool isOpen() const { return isVisible() && !closing_; }
    void setTheme(const Theme &theme);
    void dismiss();
    void refreshSnapshot();

signals:
    void chosen(int index);
    void dismissed();
    void cancelRequested();

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void hideEvent(QHideEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    QPointer<QWidget> background_, previousFocus_;
    QWidget *card_;
    QLabel *title_, *body_, *detail_;
    QLineEdit *input_;
    ProgressBar *bar_;
    ModalMotion *motion_;
    QList<QPushButton *> buttons_;
    QPixmap snapshot_, backgroundFrame_;
    Theme theme_;
    bool closing_ = false, dismissible_ = true, progressMode_ = false;
    bool backgroundWasEnabled_ = true, backgroundFrozen_ = false, backgroundUpdatesWereEnabled_ = true;
    int pendingChoice_ = -1;

    void begin(const QString &title, const QString &body, const QStringList &actions);
    void beginDismiss(int choice);
    void finishDismiss();
    void freezeBackground();
    void restoreBackgroundUpdates();
    void composeSnapshot(const QPixmap &blurred);
    void centerCard();
};

}
