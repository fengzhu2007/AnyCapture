#ifndef REGION_SELECTOR_H
#define REGION_SELECTOR_H

#include <QWidget>

namespace Ui {
class RegionSelector;
}


namespace adc{
class RegionSelectorPrivate;
class RegionSelector : public QWidget
{
    Q_OBJECT
public:
    static RegionSelector* open(QWidget* parent);


    explicit RegionSelector(QWidget *parent = nullptr);
    ~RegionSelector();
    void initResizer();
    void updateResizer();


signals:
    void confirm(int index,const QRect &rc);
public slots:
    void onOk();
    void onCancel();
    void moveTo();
    void resizeTo();

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void paintEvent(QPaintEvent* e) override;
    virtual void mousePressEvent(QMouseEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *e) override;
    virtual void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void updateMessage();

private:
    Ui::RegionSelector *ui;
    RegionSelectorPrivate* d;
    static RegionSelector* instance;
};
}
#endif // REGION_SELECTOR_H
