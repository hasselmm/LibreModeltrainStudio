#ifndef LMRS_STUDIO_MAINWINDOW_H
#define LMRS_STUDIO_MAINWINDOW_H

#include <QFrame>
#include <QMainWindow>
#include <QPointer>

namespace lmrs::core { class Device; }
namespace lmrs::core::l10n { class LanguageManager; }
namespace lmrs::widgets { class DocumentManager; }

namespace lmrs::studio {

class DeviceFilter;

class MainWindowView : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged FINAL)

public:
    enum class ActionCategory {
        FileNew,
        FileOpen,
        FileSave,
        FilePeripherals,
        EditCreate,
        EditClipboard,
        View,
    };

    Q_ENUM(ActionCategory)

    enum class FileState {
        None,
        Saved,
        Modified,
    };

    Q_ENUM(FileState)

    using QFrame::QFrame;

    virtual QList<QActionGroup *> actionGroups(ActionCategory category) const;
    virtual FileState fileState() const;
    virtual QString fileName() const;

    virtual DeviceFilter deviceFilter() const;
    void setDevice(core::Device *newDevice);
    core::Device *device() const;

    virtual bool isDetachable() const;

signals:
    void fileNameChanged(QString fileName, QPrivateSignal);
    void modifiedChanged(bool modified, QPrivateSignal);

protected:
    void connectDocumentManager(widgets::DocumentManager *manager);
    virtual void updateControls(core::Device *device);

private:
    QPointer<core::Device> m_device;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = {});

    void setLanguageManager(core::l10n::LanguageManager *languageManager);
    core::l10n::LanguageManager *languageManager() const;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_MAINWINDOW_H
