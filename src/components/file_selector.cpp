#include "file_selector.h"
#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QFileDialog>

namespace adc{
class FileSelectorPrivate{
public:
    FileSelector::Mode mode;
    QPushButton* button;
    QLineEdit* input;
};

FileSelector::FileSelector(QWidget *parent)
    : QWidget{parent}
{
    d = new FileSelectorPrivate;
    d->mode = FileSelector::File;
    auto layout = new QHBoxLayout;
    d->button = new QPushButton(this);
    d->input = new QLineEdit(this);
    layout->addWidget(d->input,1);
    layout->addWidget(d->button);
    layout->setSpacing(0);
    layout->setMargin(0);
    this->setLayout(layout);

    connect(d->button,&QPushButton::clicked,this,&FileSelector::onOpenDialog);

}

void FileSelector::setMode(FileSelector::Mode mode){
    d->mode = mode;
}



void FileSelector::onOpenDialog(){
    QString text;
    if(d->mode==FileSelector::File){
        text = QFileDialog::getExistingDirectory(this);
    }else{
        text = QFileDialog::getSaveFileName(this);
    }
    if(!text.isEmpty()){
        d->input->setText(text);
    }
}






}
