#include "mainwindow.h"

#include <QApplication>
#include <winrt/Windows.Foundation.Collections.h>

int main(int argc, char *argv[])
{
    //winrt::init_apartment();
    QApplication a(argc, argv);



    adc::MainWindow w;
    w.show();
    return a.exec();
}
