#ifndef FILE_SELECTOR_H
#define FILE_SELECTOR_H

#include <QWidget>
namespace adc{
class FileSelectorPrivate;
class FileSelector : public QWidget
{
    Q_OBJECT
public:
    enum Mode{
        File,
        Directory,
    };
    explicit FileSelector(QWidget *parent = nullptr);
    void setMode(Mode mode);

public slots:
    void onOpenDialog();

signals:
    void changed(const QString& text);
    void error();

private:
    FileSelectorPrivate* d;
};
}

#endif // FILE_SELECTOR_H
