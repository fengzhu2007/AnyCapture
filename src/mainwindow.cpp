#include "mainwindow.h"
#include "./ui_mainwindow.h"
//#include "window_capture_worker.h"
//#include "ffmpeg_muxer.h"
#include "window_enumerator.h"
#include "dxgiscreencapturer.h"
#include "components/app_select.h"
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
namespace adc{
class MainWindowPrivate{
public:
    //WindowCaptureWorker* worker = nullptr;
    //FFmpegMuxer* muxer = nullptr;
    AppSelect* appSelect = nullptr;
    WId targetWindow=0;
    QThread *thread = nullptr;

    Recorder *recorder = nullptr;


    QToolButton* close;
    int offsetX;
    int offsetY;
    bool moving=false;



    //GraphicsCapture capture;

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

    this->d = new MainWindowPrivate;
    this->d->recorder = new Recorder(this);
    this->d->close = new QToolButton(this);

    this->d->close->setMaximumSize({24,24});
    this->d->close->setStyleSheet("QToolButton{padding:0;background-color: transparent; border: none;image:url(:/res/icons/Close_24x.svg);}QToolButton:hover{background-color: transparent; border: none;image:url(:/res/icons/CloseHover_24x.svg);}");

    int fontId = QFontDatabase::addApplicationFont(":/res/fonts/DigitalNumbers.ttf");
    if (fontId != -1) {

        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        if (!fontFamilies.isEmpty()) {
            QString familyName = fontFamilies.at(0);
            QFont customFont(familyName, 12);
            ui->time->setFont(customFont);
        }
    }

    connect(this->d->recorder,&Recorder::errorOccurred,this,&MainWindow::onOutputError);
    connect(ui->start,&QToolButton::clicked,this,&MainWindow::start);
    connect(ui->stop,&QToolButton::clicked,this,&MainWindow::stop);

    connect(ui->screen,&QToolButton::clicked,this,&MainWindow::onScreenTarget);
    connect(ui->region,&QToolButton::clicked,this,&MainWindow::onRegionTarget);
    connect(ui->window,&QToolButton::clicked,this,&MainWindow::onMoreWindow);
    connect(ui->more,&QToolButton::clicked,this,&MainWindow::onMoreWindow);

    connect(ui->expand,&QToolButton::clicked,this,&MainWindow::onExpandToggle);

    connect(this->d->close,&QToolButton::clicked,this,&MainWindow::close);
    //connect(this->d->recorder,&QThread::finished,this,&MainWindow::onFinished);


}

MainWindow::~MainWindow()
{
    delete this->d;
    delete ui;
}

void MainWindow::previewCapture(){
    /*auto pixmap = d->recorder->captureImage();
    qDebug()<<"previewCapture"<<pixmap;
    //ui->image->setPixmap(pixmap);
    QSize size = ui->image->size();
    ui->image->setPixmap(pixmap.scaled({size.width(),size.height()}, Qt::KeepAspectRatio, Qt::SmoothTransformation));*/
}

void MainWindow::onStart(QAction *action){
    this->start();
}

void MainWindow::onStop(QAction *action){
    this->stop();
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

void MainWindow::start(){
    QString outputFile = "capture.mp4";
    if (this->d->recorder->start(outputFile, QSize(1920,1080), 30)) {
        qDebug()<<"start recording";
    }else{

    }
}

void MainWindow::stop(){
    if(this->d->recorder!=nullptr){
        this->d->recorder->stop();
    }
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

}
