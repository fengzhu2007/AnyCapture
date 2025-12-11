#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct WindowInfo;
namespace adc{
class MainWindowPrivate;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum State {
        Stopped,
        Recording,
        Paused
    };
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void init();
    void updateUI(State state);
    void previewCapture();

public slots:
    void start();
    void stop();

    void onScreenTarget();
    void onRegionTarget();
    void onWindowTarget();
    void onMoreWindow();
    void onOutputError(const QString& message);
    void onFinished();
    void onExpandToggle();
    void onAppSelected(const WindowInfo& info);
    void onWindowPopupClosed();
    void onTimeout();
    void onToggleSound();
    void onToggleMircophone();
    void onOpenOutput(const QString& path);

private:
    QString formattedTime(qint64 ms);
    void initResolution();
    void initFPS();
    QString outputFilename() const;

protected:
    virtual void resizeEvent(QResizeEvent* e) override;
    virtual void paintEvent(QPaintEvent* e) override;
    virtual void mousePressEvent(QMouseEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *e) override;
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
private:
    Ui::MainWindow *ui;
    MainWindowPrivate* d;

};
}
#endif // MAINWINDOW_H
