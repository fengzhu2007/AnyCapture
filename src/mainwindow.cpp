#include "mainwindow.h"
#include "./ui_mainwindow.h"
//#include "window_capture_worker.h"
//#include "ffmpeg_muxer.h"
#include "window_enumerator.h"
#include "components/app_select.h"
#include "screen_recorder.h"
//#include "windowcapture.h"
#include <QDebug>
#include <QThread>
#include "screen_recorder.h"
#include "recorder.h"
#include <QToolButton>
#include <QResizeEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QMessageBox>
#include <QElapsedTimer>
namespace adc{
class MainWindowPrivate{
public:
    AppSelect* appSelect = nullptr;
    WId targetWindow=0;
    Recorder *recorder = nullptr;
    //ScreenRecorder* screenRecorder = nullptr;
    QToolButton* close;
    int offsetX;
    int offsetY;
    bool moving=false;

    MainWindow::State state;
    bool recording = false;
    bool pause = false;
    QElapsedTimer elapsedTimer;
    QTimer timer;
    qint64 totalTime = 0;
    qint64 pauseTime = 0;
    qint64 timeCont = 0;

};


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);


    ui->setupUi(this);

    this->setStyleSheet("QToolButton{padding:0;background-color: transparent; border: none;border-radius:2px;}"
                        "QToolButton:hover{background-color:#333}"
                        "QToolButton:checked{background-color:#3e3e3e}"
                        "#time{color:#ffffff;font-size:14px;}"
                        "#widget .QLabel{color:#fff;}");

    d = new MainWindowPrivate;
    d->state = Stopped; 
    d->timer.setInterval(1000);
    d->recorder = new Recorder(this);
    d->close = new QToolButton(this);

    d->close->setMaximumSize({24,24});
    d->close->setStyleSheet("QToolButton{padding:0;background-color: transparent; border: none;image:url(:/res/icons/Close_24x.svg);}QToolButton:hover{background-color: transparent; border: none;image:url(:/res/icons/CloseHover_24x.svg);}");

    int fontId = QFontDatabase::addApplicationFont(":/res/fonts/DigitalNumbers.ttf");
    if (fontId != -1) {

        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        if (!fontFamilies.isEmpty()) {
            QString familyName = fontFamilies.at(0);
            QFont customFont(familyName, 12);
            ui->time->setFont(customFont);
        }
    }

    connect(d->recorder,&Recorder::errorOccurred,this,&MainWindow::onOutputError);
    connect(ui->start,&QToolButton::clicked,this,&MainWindow::start);
    connect(ui->stop,&QToolButton::clicked,this,&MainWindow::stop);

    connect(ui->screen,&QToolButton::clicked,this,&MainWindow::onScreenTarget);
    connect(ui->region,&QToolButton::clicked,this,&MainWindow::onRegionTarget);
    connect(ui->window,&QToolButton::clicked,this,&MainWindow::onMoreWindow);
    connect(ui->more,&QToolButton::clicked,this,&MainWindow::onMoreWindow);

    connect(ui->expand,&QToolButton::clicked,this,&MainWindow::onExpandToggle);

    connect(this->d->close,&QToolButton::clicked,this,&MainWindow::close);
    connect(&d->timer, &QTimer::timeout, this, &MainWindow::onTimeout);
    //connect(this->d->recorder,&QThread::finished,this,&MainWindow::onFinished);
    ui->time->setText(this->formattedTime(0));
    this->onScreenTarget();

}

MainWindow::~MainWindow()
{
    d->recorder->stop();
    
    delete this->d;
    delete ui;
}

void MainWindow::updateUI(State state) {
    if (state == Stopped) {
        ui->start->setIcon(QIcon(":/res/icons/Run_32x.svg"));
        ui->start->setToolTip(tr("Start"));
        ui->time->setText("00:00:00");
        ui->time->setStyleSheet("color: rgba(255, 255, 255, 1)");
        ui->stop->setEnabled(false);
    }
    else if (state == Recording) {
        ui->start->setIcon(QIcon(":/res/icons/Pause_32x.svg"));
        ui->start->setToolTip(tr("Pause"));
        ui->time->setStyleSheet("color: rgba(255, 255, 255, 1)");
        ui->stop->setEnabled(true);
    }
    else if (state == Paused) {
        ui->start->setIcon(QIcon(":/res/icons/Run_32x.svg"));
        ui->start->setToolTip(tr("Resume"));
        ui->stop->setEnabled(true);
    }
}

void MainWindow::previewCapture(){
    /*auto pixmap = d->recorder->captureImage();
    qDebug()<<"previewCapture"<<pixmap;
    //ui->image->setPixmap(pixmap);
    QSize size = ui->image->size();
    ui->image->setPixmap(pixmap.scaled({size.width(),size.height()}, Qt::KeepAspectRatio, Qt::SmoothTransformation));*/
}



void MainWindow::onScreenTarget(){
    ui->screen->setChecked(true);
    ui->region->setChecked(false);
    ui->window->setChecked(false);
    d->recorder->setTargetWindow(0);
    this->previewCapture();
}

void MainWindow::onRegionTarget(){
    ui->screen->setChecked(false);
    ui->window->setChecked(false);
    ui->region->setChecked(true);
}

void MainWindow::onWindowTarget(){
    ui->screen->setChecked(false);
    ui->region->setChecked(false);
    ui->window->setChecked(true);
}

void MainWindow::onMoreWindow(){
    if(d->appSelect==nullptr){
        d->appSelect = new AppSelect(this);
        connect(d->appSelect,&AppSelect::selected,this,&MainWindow::onAppSelected);
        connect(d->appSelect,&AppSelect::closed,this,&MainWindow::onWindowPopupClosed);
        d->appSelect->show();

    }else{
        d->appSelect->init();
        d->appSelect->show();
    }
    QPoint globalPos = ui->window->mapToGlobal(QPoint(0, 0));
    int x = globalPos.x();
    int y = globalPos.y() + ui->window->height();
    d->appSelect->move(x, y);
}

void MainWindow::onOutputError(const QString& message){
    qDebug()<<"record error:"<<message;
}

void MainWindow::onFinished(){
    this->d->recorder->deleteLater();
    this->d->recorder = nullptr;
}

void MainWindow::onExpandToggle(){
    auto ret = ui->widget->isVisible();
    auto rect = this->geometry();
    if(!ret){
        ui->widget->show();
        rect.setHeight(200);
        this->setGeometry(rect);
        this->setMaximumHeight(500);
    }else{
        ui->widget->hide();
        rect.setHeight(30);
        this->setGeometry(rect);
        this->setMaximumHeight(30);
    }
    this->updateGeometry();
    this->adjustSize();
}

void MainWindow::onAppSelected(const WindowInfo& info){
    if(!info.icon.isNull()){
        ui->window->setIcon(info.icon.pixmap(24,24));
        ui->window->setToolTip(info.title);
        d->targetWindow = info.handle;
        d->recorder->setTargetWindow(d->targetWindow);
        this->onWindowTarget();
    }
}

void MainWindow::onWindowPopupClosed(){
    auto rect = ui->more->geometry();
    QMouseEvent moveEvent(QEvent::Leave, QPoint(0,0), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(ui->more, &moveEvent);
}

void MainWindow::onTimeout() {
    d->timeCont += 1;
    if (d->state == Paused) {
        if (d->timeCont % 2 == 0) {
            ui->time->setStyleSheet("color: rgba(255, 255, 255, 0.6)");
        }
        else {
            ui->time->setStyleSheet("color: rgba(255, 255, 255, 1)");
        }
    }
    else {
        auto len = d->elapsedTimer.elapsed();
        auto total = len + d->totalTime;
        ui->time->setText(this->formattedTime(total));
    }

}



void MainWindow::start(){
    if (d->state == Stopped) {
        QString outputFile = "capture.mp4";
        if (d->recorder != nullptr) {
            if (d->recorder->start(outputFile, QSize(1920, 1080), 30)) {
                ui->start->setIcon(QIcon(":/res/icons/Pause_32x.svg"));
                d->state = Recording;
                d->timer.start();
                d->elapsedTimer.start();
            }else{
                qDebug() << "recording start failed";
                return;
            }
        }
    }
    else if (d->state == Recording) {
        d->recorder->pause();
        d->state = Paused;
        d->totalTime = d->elapsedTimer.elapsed();
    }
    else if (d->state == Paused) {
        d->recorder->resume();
        d->state = Recording;
        d->elapsedTimer.restart();
    }
    this->updateUI(d->state);
}



void MainWindow::stop(){
   if (this->d->recorder != nullptr) {
        this->d->recorder->stop();
    }
   d->state = Stopped;
   d->totalTime = 0;
   d->timer.stop();
   this->updateUI(d->state);
}

void MainWindow::resizeEvent(QResizeEvent* e){
    auto size = e->size();
    this->d->close->setGeometry({size.width() - 24,0,24,24});
    QMainWindow::resizeEvent(e);
}

void MainWindow::paintEvent(QPaintEvent *e){
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0, 0, 0, 0));
    painter.setPen(Qt::NoPen);
    QRect rc = this->rect();
    QPainterPath path;
    path.addRoundedRect(QRect(0, 0, rc.width(),rc.height()), 4, 4);
    painter.fillPath(path, QColor("#222222"));
}

void MainWindow::mouseMoveEvent(QMouseEvent *e)
{
    QWidget::mouseMoveEvent(e);
    if(d->moving){
        int x = e->x() ;
        int y = e->y();
        x -= d->offsetX;
        y -= d->offsetY;
        QPoint pos = this->pos();
        pos.rx() += x;
        pos.ry() += y;
        this->move(pos);
    }else{
        if(abs(d->offsetX - e->x())>3 || abs(d->offsetY-e->y())>3){
            d->moving = true;
        }
        this->unsetCursor();
    }
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    QWidget::mousePressEvent(e);
    d->offsetX = e->x();
    d->offsetY = e->y();
}

void MainWindow::mouseReleaseEvent(QMouseEvent *e)
{
    QWidget::mouseReleaseEvent(e);
    d->moving = false;
}

QString MainWindow::formattedTime(qint64 ms)
{
    qDebug() << "ms:" << ms;
    int seconds = ms / 1000;
    int minutes = seconds / 60;
    int hours = minutes / 60;

    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}
}
