#ifndef ESU_PROGRAMMER_MAINWINDOW_H
#define ESU_PROGRAMMER_MAINWINDOW_H

#include <QMainWindow>

namespace esu::programmer {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    class Private;
    Private *const d;
};

} // namespace esu::programmer

#endif // ESU_PROGRAMMER_MAINWINDOW_H
