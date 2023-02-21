#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = {});

public slots:
    void open(QString filePath);

private:
    class Private;
    Private *const d;
};

#endif // MAINWINDOW_H
