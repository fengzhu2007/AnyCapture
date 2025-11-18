#include "window_enumerator.h"
#include <QApplication>
#include <QBuffer>
#include <QtWinExtras/QtWin>
#include <QDebug>

HICON getWindowIconSafe(HWND hwnd)
{
    HICON hIcon = nullptr;

    hIcon = reinterpret_cast<HICON>(
        SendMessage(hwnd, WM_GETICON, ICON_BIG, 0));
    if (!hIcon)
        hIcon = reinterpret_cast<HICON>(
            SendMessage(hwnd, WM_GETICON, ICON_SMALL2, 0));
    if (!hIcon)
        hIcon = reinterpret_cast<HICON>(GetClassLongPtr(hwnd, GCLP_HICON));
    if (!hIcon)
        hIcon = reinterpret_cast<HICON>(
            SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0));
    if (!hIcon)
        hIcon = reinterpret_cast<HICON>(GetClassLongPtr(hwnd, GCLP_HICONSM));
    return hIcon;
}

QPixmap pixmapFromHWNDIcon(HWND hwnd)
{
    HICON hIcon = getWindowIconSafe(hwnd);
    if (!hIcon) return QPixmap();

    HICON hCopy = CopyIcon(hIcon);
    if (!hCopy) return QPixmap();

    QPixmap pix = QtWin::fromHICON(hCopy);
    DestroyIcon(hCopy);
    return pix;
}



static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    QList<WindowInfo> *list = reinterpret_cast<QList<WindowInfo> *>(lParam);

    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t title[512];
    GetWindowTextW(hwnd, title, sizeof(title)/sizeof(wchar_t));
    if (wcslen(title) == 0) return TRUE;

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (!(style & WS_OVERLAPPEDWINDOW) && !(style & WS_POPUP))
        return TRUE;



    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));

    if (strcmp(className, "Progman") == 0 ||
        strcmp(className, "Shell_TrayWnd") == 0 ||
        strcmp(className, "Button") == 0 ||
        strcmp(className, "Static") == 0 ||
        strcmp(className, "Windows.UI.Core.CoreWindow")==0
        )
        return TRUE;

    DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    QPixmap pix = pixmapFromHWNDIcon(hwnd);
    if(pix.isNull()==false){
        QIcon icon(pix.scaled({32,32}));
        WindowInfo info;
        info.handle = reinterpret_cast<WId>(hwnd);
        info.title = QString::fromWCharArray(title);
        info.icon = icon;
        list->append(info);

        qDebug()<<info.title<<className;
    }
    return TRUE;
}


namespace adc{


QList<WindowInfo> WindowEnumerator::enumerateWindows()
{
    QList<WindowInfo> list;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&list));
    return list;
}

}
