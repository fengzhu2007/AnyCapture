#ifndef APP_SELECT_H
#define APP_SELECT_H

#include <QWidget>
#include <QListWidgetItem>
#include "../window_enumerator.h"
namespace adc{
class AppSelectPrivate;
class AppSelect : public QWidget
{
    Q_OBJECT
public:
    explicit AppSelect(QWidget *parent = nullptr);
    void init();
protected:
    void hideEvent(QHideEvent *event);


public slots:
    void onItemClicked(QListWidgetItem *item);
signals:
    void selected(const WindowInfo& info);
    void closed();

private:
    AppSelectPrivate* d;
};
}
#endif // APP_SELECT_H
