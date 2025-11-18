#ifndef WINDOW_ENUMERATOR_H
#define WINDOW_ENUMERATOR_H


#include <QObject>
#include <QList>
#include <QIcon>
#include <QString>



#include <windows.h>


struct WindowInfo {
    WId handle;      // HWND
    QString title;
    QIcon icon;
};
Q_DECLARE_METATYPE(WindowInfo)


namespace adc{






class WindowEnumerator
{
public:
   static QList<WindowInfo> enumerateWindows();
};
}

#endif // WINDOW_ENUMERATOR_H
