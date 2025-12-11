#include "region_selector.h"
#include "src/components/ui_region_selector.h"
#include "ui_region_selector.h"
#include "window_resizer.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScreen>
#include <QDebug>
namespace adc{

class RegionSelectorPrivate{
public:
    WindowResizer* regions[8];
    int resizer_size = 6;
    bool moving = false;
    int offsetX;
    int offsetY;

};

RegionSelector* RegionSelector::instance = nullptr;

RegionSelector* RegionSelector::open(QWidget* parent){
    if(instance==nullptr){
        instance = new RegionSelector(parent);
    }
    instance->show();
    return instance;
}


RegionSelector::RegionSelector(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RegionSelector)
{
    //setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    this->setStyleSheet("QLabel{color:#fff;}"
                        "QToolButton{padding:0;background-color: #222; border: none;border-radius:2px;}"
                        "QToolButton:hover,QPushButton:hover{background-color:#333}"
                        "QToolButton:checked{background-color:#3e3e3e}"
                        "QPushButton{padding:6px 10px;background:#333;color:#fff;border-radius:2px;}"
                        "QPushButton:hover{background-color:#3e3e3e;}"
                        );
    ui->setupUi(this);

    ui->close->setMaximumSize({24,24});
    ui->close->setStyleSheet("QToolButton{padding:0;background-color: transparent; border: none;image:url(:/res/icons/Close_24x.svg);}QToolButton:hover{background-color: transparent; border: none;image:url(:/res/icons/CloseHover_24x.svg);}");



    connect(ui->close,&QToolButton::clicked,this,&RegionSelector::close);
    connect(ui->ok,&QPushButton::clicked,this,&RegionSelector::onOk);
    connect(ui->cancel,&QToolButton::clicked,this,&RegionSelector::onCancel);
    connect(ui->left,&QToolButton::clicked,this,&RegionSelector::moveTo);
    connect(ui->leftTop,&QToolButton::clicked,this,&RegionSelector::moveTo);
    connect(ui->leftBottom,&QToolButton::clicked,this,&RegionSelector::moveTo);
    connect(ui->top,&QToolButton::clicked,this,&RegionSelector::moveTo);
    connect(ui->center,&QToolButton::clicked,this,&RegionSelector::moveTo);
    connect(ui->bottom,&QToolButton::clicked,this,&RegionSelector::moveTo);
    connect(ui->rightTop,&QToolButton::clicked,this,&RegionSelector::moveTo);
    connect(ui->right,&QToolButton::clicked,this,&RegionSelector::moveTo);
    connect(ui->rightBottom,&QToolButton::clicked,this,&RegionSelector::moveTo);


    connect(ui->resize_800_600,&QToolButton::clicked,this,&RegionSelector::resizeTo);
    connect(ui->resize_1280_720,&QToolButton::clicked,this,&RegionSelector::resizeTo);
    connect(ui->resize_1920_1080,&QToolButton::clicked,this,&RegionSelector::resizeTo);


    d = new RegionSelectorPrivate;

    this->initResizer();
    this->raise();

}

RegionSelector::~RegionSelector()
{
    RegionSelector::instance = nullptr;
    delete d;
    delete ui;
}


void RegionSelector::initResizer(){
    for(int i=0;i<8;i++){
        WindowResizer::Region region = (WindowResizer::Region)i;
        d->regions[i] = new WindowResizer(this,region);
        d->regions[i]->raise();
    }
    updateResizer();
}


void RegionSelector::updateResizer()
{
    QRect rc = geometry();
    for(int i=0;i<8;i++){

        WindowResizer::Region region = (WindowResizer::Region)i;
        if(region==WindowResizer::Left){
            d->regions[i]->setGeometry(0,d->resizer_size,d->resizer_size,rc.height() - 2*d->resizer_size);
        }else if(region==WindowResizer::Right){
            d->regions[i]->setGeometry(rc.width() - d->resizer_size,d->resizer_size,d->resizer_size,rc.height() - 2*d->resizer_size);
        }else if(region==WindowResizer::Top){
            d->regions[i]->setGeometry(d->resizer_size,0,rc.width() - 2*d->resizer_size,d->resizer_size);
        }else if(region==WindowResizer::Bottom){
            d->regions[i]->setGeometry(d->resizer_size,rc.height() - d->resizer_size,rc.width() - 2*d->resizer_size,d->resizer_size);
        }else if(region==WindowResizer::LeftTop){
            d->regions[i]->setGeometry(0,0,d->resizer_size,d->resizer_size);
        }else if(region==WindowResizer::LeftBottom){
            d->regions[i]->setGeometry(0,rc.height() - d->resizer_size,d->resizer_size,d->resizer_size);
        }else if(region==WindowResizer::RightTop){
            d->regions[i]->setGeometry(rc.width() - d->resizer_size,0,d->resizer_size,d->resizer_size);
        }else if(region==WindowResizer::RightBottom){
            d->regions[i]->setGeometry(rc.width() - d->resizer_size,rc.height() - d->resizer_size,d->resizer_size,d->resizer_size);
        }

    }

}


void RegionSelector::onOk(){
    emit confirm(0,this->geometry());
    this->close();
}


void RegionSelector::onCancel(){
    this->close();
}

void RegionSelector::moveTo(){
    auto sender = this->sender();
    QPoint pos;
    QSize size = this->size();
    QScreen *currentScreen = this->screen();
    auto screenSize = currentScreen->size();

    if(sender==ui->left){
        pos = {0,(screenSize.height() - size.height()) / 2};
    }else if(sender==ui->leftTop){
        pos = {0,0};
    }else if(sender==ui->leftBottom){
        pos = {0,screenSize.height() - size.height()};
    }else if(sender==ui->top){
        pos = {(screenSize.width() - size.width()) / 2,0};
    }else if(sender==ui->center){
        pos = {(screenSize.width() - size.width()) / 2,(screenSize.height() - size.height()) / 2 };
    }else if(sender==ui->bottom){
        pos = {(screenSize.width() - size.width()) / 2,screenSize.height() - size.height() };
    }else if(sender==ui->rightTop){
        pos = {screenSize.width() - size.width() ,0 };
    }else if(sender==ui->right){
        pos = {screenSize.width() - size.width() ,(screenSize.height() - size.height()) / 2 };
    }else if(sender==ui->rightBottom){
        pos = {screenSize.width() - size.width() ,screenSize.height() - size.height() };
    }else{
        return ;
    }
    //qDebug()<<sender<<pos<<screenSize;
    this->move(pos);
}

void RegionSelector::resizeTo(){
    auto sender = this->sender();
    QSize size;
    if(sender==ui->resize_800_600){
        size = {800,600};
    }else if(sender==ui->resize_1280_720){
        size = {1280,720};
    }else if(sender==ui->resize_1920_1080){
        size = {1920,1080};
    }else{
        return ;
    }
    auto rc = this->geometry();
    rc.setSize(size);
    this->setGeometry(rc);
}


void RegionSelector::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResizer();
    this->updateMessage();
}


void RegionSelector::paintEvent(QPaintEvent *e){
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0, 0, 0, 0));
    painter.setPen(Qt::NoPen);
    QRect rc = this->rect();
    QPainterPath path;
    path.addRoundedRect(QRect(0, 0, rc.width(),rc.height()), 4, 4);
    painter.fillPath(path, QColor("#aa222222"));

    QPen pen(QColor("#aa1677FF"));
    pen.setWidth(4);
    pen.setStyle(Qt::SolidLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(0,0,this->size().width(),this->size().height());

}

void RegionSelector::mouseMoveEvent(QMouseEvent *e)
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
        this->updateMessage();
    }else{

    }
}

void RegionSelector::mousePressEvent(QMouseEvent *e)
{
    QWidget::mousePressEvent(e);

    auto rc = this->geometry();
    auto x = e->x();
    auto y = e->y();
    if(x<=d->resizer_size || y<=d->resizer_size || x>=rc.width() - d->resizer_size || y>=rc.height() - d->resizer_size){
        return ;
    }
    if(abs(d->offsetX - e->x())>3 || abs(d->offsetY-e->y())>3){
        d->moving = true;
    }
    this->unsetCursor();
    d->offsetX = e->x();
    d->offsetY = e->y();
}

void RegionSelector::mouseReleaseEvent(QMouseEvent *e)
{
    QWidget::mouseReleaseEvent(e);

    d->moving = false;
}

void RegionSelector::updateMessage(){
    auto rc = this->geometry();
    ui->message->setText(tr("Left:%1 Top:%2 Width:%3 Height:%4").arg(rc.left()).arg(rc.top()).arg(rc.width()).arg(rc.height()));
}

}
