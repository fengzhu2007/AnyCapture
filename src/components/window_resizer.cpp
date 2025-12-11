#include "window_resizer.h"
#include <QMouseEvent>
#include <QDebug>

namespace adc{
    class WindowResizerPrivate {
    public:
        WindowResizer::Region region;

        bool moving = false;
        int offsetX = 0;
        int offsetY = 0;
    };

    WindowResizer::WindowResizer(QWidget* parent,Region region)
        :QFrame(parent)
    {
        d = new WindowResizerPrivate;
        d->region = region;
        switch(d->region){
        case Left:
        case Right:
            this->setCursor(Qt::SizeHorCursor);
            break;
        case Top:
        case Bottom:
            this->setCursor(Qt::SizeVerCursor);
            break;
        case LeftTop:
        case RightBottom:
            this->setCursor(Qt::SizeFDiagCursor);
            break;
        case LeftBottom:
        case RightTop:
            this->setCursor(Qt::SizeBDiagCursor);
            break;
        }
        //this->setAttribute(Qt::WA_TranslucentBackground);


    }

    WindowResizer::~WindowResizer()
    {
        delete d;
    }

    void WindowResizer::setRegion(Region region)
    {
        d->region = region;
        switch(d->region){
        case Left:
        case Right:
            this->setCursor(Qt::SizeHorCursor);
            break;
        case Top:
        case Bottom:
            this->setCursor(Qt::SizeVerCursor);
            break;
        case LeftTop:
        case RightBottom:
            this->setCursor(Qt::SizeFDiagCursor);
            break;
        case LeftBottom:
        case RightTop:
            this->setCursor(Qt::SizeBDiagCursor);
            break;
        }
    }

    WindowResizer::Region WindowResizer::region()
    {
        return d->region;
    }

    void WindowResizer::mouseMoveEvent(QMouseEvent *event)
    {
        QWidget::mouseMoveEvent(event);
        if(d->moving){
            QWidget* parent = (QWidget*)parentWidget();
            if(parent!=nullptr){
                    int x = event->x() ;
                    int y = event->y();
                    x -= d->offsetX;
                    y -= d->offsetY;
                    QPoint pos = parent->pos();
                    pos.rx() += x;
                    pos.ry() += y;
                    QRect rc = parent->geometry();
                    if(d->region==WindowResizer::Left || d->region==WindowResizer::LeftTop || d->region==WindowResizer::LeftBottom){
                        rc.setX(rc.x() + x);
                    }
                    if(d->region==WindowResizer::Right || d->region==WindowResizer::RightTop || d->region==WindowResizer::RightBottom){
                        rc.setWidth(rc.width() + x);
                    }
                    if(d->region==WindowResizer::Top || d->region==WindowResizer::LeftTop || d->region==WindowResizer::RightTop){
                        rc.setY(rc.y() + y);
                    }
                    if(d->region==WindowResizer::Bottom || d->region==WindowResizer::LeftBottom || d->region==WindowResizer::RightBottom){
                        rc.setHeight(rc.height() + y);
                    }
                    qDebug()<<"update rect:"<<rc;
                    parent->setGeometry(rc);
                    //parent->updateResizer();
            }
        }
    }

    void WindowResizer::mousePressEvent(QMouseEvent *e)
    {
        QWidget::mousePressEvent(e);
        d->moving = true;
        d->offsetX = e->x();
        d->offsetY = e->y();
    }

    void WindowResizer::mouseReleaseEvent(QMouseEvent *e)
    {
        QWidget::mouseReleaseEvent(e);
        d->moving = false;
    }

    }
