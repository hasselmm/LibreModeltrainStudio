#ifndef LMRS_STUDIO_MAINWINDOW_H
#define LMRS_STUDIO_MAINWINDOW_H

#include <QFrame>
#include <QMainWindow>

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
        EditCreate,
        EditClipboard,
        View,
    };

    Q_ENUM(ActionCategory)

    using QFrame::QFrame;

    virtual QActionGroup *actionGroup(ActionCategory category) const;
    virtual QString fileName() const;
    virtual bool isModified() const;

    virtual void setCurrentDevice(core::Device *device);
    virtual DeviceFilter deviceFilter() const;

    virtual bool isDetachable() const;

signals:
    void fileNameChanged(QString fileName, QPrivateSignal);
    void modifiedChanged(bool modified, QPrivateSignal);

protected:
    void connectDocumentManager(widgets::DocumentManager *manager);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = {});
    ~MainWindow() override;

    void setLanguageManager(core::l10n::LanguageManager *languageManager);
    core::l10n::LanguageManager *languageManager() const;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_MAINWINDOW_H
