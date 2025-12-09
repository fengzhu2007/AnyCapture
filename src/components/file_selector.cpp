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
    d->button->setObjectName("file-selector-button");
    d->input = new QLineEdit(this);
    d->input->setObjectName("file-selector-input");
    layout->addWidget(d->input,1);
    layout->addWidget(d->button);
    layout->setSpacing(6);
    layout->setMargin(0);
    this->setLayout(layout);

    connect(d->button,&QPushButton::clicked,this,&FileSelector::onOpenDialog);

    d->button->setIcon(QIcon(":/res/icons/SelectFile_24x.svg"));

    this->setStyleSheet("#file-selector-button{background-color:#3e3e3e;border:1px solid #222;border-radius:2px;height:24px;}"
                        "#file-selector-button:hover{background-color:#444;}"
                        "#file-selector-input{background-color:#3e3e3e;border:1px solid #222;border-radius:2px;height:24px;color:#fff;}");

}

void FileSelector::setMode(FileSelector::Mode mode){
    d->mode = mode;
}


void FileSelector::setText(const QString& text){
    d->input->setText(text);
}

QString FileSelector::text(){
    return d->input->text();
}

void FileSelector::onOpenDialog(){
    QString text;
    if(d->mode==FileSelector::File){
        text = QFileDialog::getExistingDirectory(this,{},d->input->text());
    }else{
        text = QFileDialog::getSaveFileName(this,{},d->input->text());
    }
    if(!text.isEmpty()){
        d->input->setText(text);
    }
}






}
