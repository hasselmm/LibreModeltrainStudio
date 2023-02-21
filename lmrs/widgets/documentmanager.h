#ifndef LMRS_WIDGETS_FILEMANAGER_H
#define LMRS_WIDGETS_FILEMANAGER_H

#include <lmrs/core/memory.h>

#include <QDialog>

namespace lmrs::core {
struct FileFormat;
class FileFormatHandler;
} // namespace lmrs::core

namespace lmrs::widgets {

class RecentFileMenu;

class DocumentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged FINAL)
    Q_PROPERTY(bool isModified READ isModified WRITE setModified RESET resetModified NOTIFY modifiedChanged FINAL)
    Q_PROPERTY(QString documentType READ documentType CONSTANT FINAL)

public:
    explicit DocumentManager(QString settingsKey, QWidget *parent = nullptr);

    virtual QString documentType() const = 0;
    virtual QList<core::FileFormat> readableFileFormats() const = 0;
    virtual QList<core::FileFormat> writableFileFormats() const = 0;

    QString fileName() const;

    const RecentFileMenu *recentFilesMenu() const { return m_recentFilesMenu; }
    RecentFileMenu *recentFilesMenu() { return m_recentFilesMenu; }

    void setModified(bool newModified = true);
    void markModified();
    void resetModified();
    bool isModified() const;

public slots:
    void reset();
    void open();
    void openWithFileName(QString newFileName);
    void save();
    void saveAs();

signals:
    void fileNameChanged(QString fileName, QPrivateSignal);
    void modifiedChanged(bool modified, QPrivateSignal);

protected:
    using FileHandlerPointer = std::unique_ptr<core::FileFormatHandler>;

    virtual FileHandlerPointer readFile(QString fileName) = 0;
    virtual FileHandlerPointer writeFile(QString fileName) = 0;
    virtual void resetModel() = 0;

private:
    QWidget *parentWidget() const;

    QDialog::DialogCode maybeSaveChanges();
    QDialog::DialogCode saveChanges();

    QString m_fileName;
    bool m_modified = false;

    core::ConstPointer<RecentFileMenu> m_recentFilesMenu;
};

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_FILEMANAGER_H
