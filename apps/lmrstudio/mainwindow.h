#ifndef LMRS_STUDIO_MAINWINDOW_H
#define LMRS_STUDIO_MAINWINDOW_H

#include <QFrame>
#include <QMainWindow>

namespace lmrs::studio {

class MainWindowView : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged FINAL)

public:
    enum class ActionCategory {
        FileNew,
        FileOpen,
        FileSave,
        EditCreate,
        EditClipboard,
        View,
    };

    Q_ENUM(ActionCategory)

    using QFrame::QFrame;

    virtual QActionGroup *actionGroup(ActionCategory category) const;
    virtual QString fileName() const;

signals:
    void fileNameChanged(QString fileName, QPrivateSignal);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = {});
    ~MainWindow() override;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_MAINWINDOW_H
