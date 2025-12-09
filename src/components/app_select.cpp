#include "app_select.h"
#include <QListView>
#include <QListWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
namespace adc{
class AppSelectPrivate{
public:
    QListWidget* widget;
    QList<WindowInfo> list;
    WId wid;
};

AppSelect::AppSelect(QWidget *parent)
    : QWidget{parent}
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    //setAttribute(Qt::WA_DeleteOnClose);
    this->setStyleSheet("QWidget{background-color:#222222;}"
                        "QScrollBar{width:8px;background:red;}"
                        "QScrollBar::handle{background: #3E3E3E}"
                        "QScrollBar::add-line:vertical{background: red}"
                        "QScrollBar::sub-line:vertical{background: red}"
                        "QScrollBar::up-arrow:vertical{background: #8E8E8E}"
                        "QScrollBar::down-arrow:vertical {background: #8E8E8E}");
    d = new AppSelectPrivate;
    d->widget = new QListWidget(this);
    d->widget->setIconSize({32,32});
    connect(d->widget,&QListWidget::itemClicked,this,&AppSelect::onItemClicked);
    auto layout = new QHBoxLayout;
    layout->setMargin(1);
    layout->addWidget(d->widget,1);
    this->setLayout(layout);
    this->init();
    setFixedSize(60, 150);
}


void AppSelect::init(){
    d->widget->clear();
    d->list = WindowEnumerator::enumerateWindows();
    for(auto one:d->list){
        if(!one.icon.isNull()){
            auto item = new QListWidgetItem(one.icon,"",d->widget);
            item->setToolTip(one.title);
            d->widget->addItem(item);
            if(one.handle==d->wid){
                d->widget->setCurrentItem(item);
            }
        }
    }
}

void AppSelect::hideEvent(QHideEvent *event){
    QWidget::hideEvent(event);
    emit closed();
}


void AppSelect::onItemClicked(QListWidgetItem *item){
    int row = d->widget->row(item);
    //qDebug()<<"row:"<<row;
    d->wid = d->list.at(row).handle;
    emit selected(d->list.at(row));
    this->hide();
}

}
