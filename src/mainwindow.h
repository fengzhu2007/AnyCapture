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
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


    void previewCapture();

public slots:
    void start();
    void stop();
    void onStart(QAction *action);
    void onStop(QAction *action);
    void onScreenTarget();
    void onRegionTarget();
    void onWindowTarget();
    void onMoreWindow();
    void onOutputError(const QString& message);
    void onFinished();
    void onExpandToggle();
    void onAppSelected(const WindowInfo& info);
    void onWindowPopupClosed();


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
