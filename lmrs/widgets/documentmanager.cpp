#include "documentmanager.h"

#include "recentfilemenu.h"

#include <lmrs/core/fileformat.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <QFileDialog>
#include <QMessageBox>

namespace lmrs::widgets {

DocumentManager::DocumentManager(QString settingsKey, QWidget *parent)
    : QObject{parent}
    , m_recentFilesMenu{settingsKey + "/RecentFiles"_L1, parent}
{
    connect(m_recentFilesMenu, &RecentFileMenu::fileSelected,
            this, &DocumentManager::openWithFileName);
}

QWidget *DocumentManager::parentWidget() const
{
    return core::checked_cast<QWidget *>(parent());
}

QString DocumentManager::fileName() const
{
    return m_fileName;
}

void DocumentManager::setModified(bool newModified)
{
    if (std::exchange(m_modified, newModified) != m_modified)
        emit modifiedChanged(m_modified, {});
}

void DocumentManager::markModified()
{
    setModified(true);
}

void DocumentManager::resetModified()
{
    setModified(false);
}

bool DocumentManager::isModified() const
{
    return m_modified;
}

void DocumentManager::reset()
{
    if (maybeSaveChanges() == QDialog::Rejected)
        return;

    m_fileName.clear();
    emit fileNameChanged(m_fileName, {});

    resetModel();
    resetModified();
}

void DocumentManager::open()
{
    openWithFileName({});
}

void DocumentManager::openWithFileName(QString newFileName)
{
    if (maybeSaveChanges() == QDialog::Rejected)
        return;

    const auto fileNameGuard = core::propertyGuard(this, &DocumentManager::fileName, &DocumentManager::fileNameChanged);

    if (newFileName.isEmpty()) {
        auto filters = core::FileFormat::openFileDialogFilter(readableFileFormats());
        newFileName = QFileDialog::getOpenFileName(parentWidget(), tr("Open %1").arg(documentType()),
                                                   {} /* FIXME: set workdir */, std::move(filters));
    }

    if (newFileName.isEmpty())
        return;

    if (const auto fileHandler = readFile(std::move(newFileName)); fileHandler->failed()) {
        const auto fileInfo = QFileInfo{fileHandler->fileName()};

        QMessageBox::warning(parentWidget(), tr("Reading failed"),
                             tr("Reading the %1 from <b>\"%2\"</b> has failed.<br><br>%3").
                             arg(documentType(), fileInfo.fileName(), fileHandler->errorString()));
    } else {
        m_fileName = fileHandler->fileName();
        m_recentFilesMenu->addFileName(m_fileName);
        resetModified();
    }
}

void DocumentManager::save()
{
    saveChanges();
}

void DocumentManager::saveAs()
{
    auto restoreFileName = qScopeGuard([this, oldFileName = m_fileName] {
        m_fileName = std::move(oldFileName);
    });

    m_fileName.clear();
    emit fileNameChanged(m_fileName, {});

    if (saveChanges() == QDialog::Accepted)
        restoreFileName.dismiss();
}

QDialog::DialogCode DocumentManager::maybeSaveChanges()
{
    if (!isModified())
        return QDialog::Accepted;

    const auto result = QMessageBox::question(parentWidget(), tr("Save changes"),
                                              tr("There are unsaved changes to your %1. "
                                                 "Do you want to save them first?").arg(documentType()),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (result == QMessageBox::Cancel)
        return QDialog::Rejected;

    if (result == QMessageBox::Discard)
        return QDialog::Accepted;

    return saveChanges();
}

QDialog::DialogCode DocumentManager::saveChanges()
{
    const auto fileNameGuard = core::propertyGuard(this, &DocumentManager::fileName, &DocumentManager::fileNameChanged);

    if (m_fileName.isEmpty()) {
        auto filters = core::FileFormat::saveFileDialogFilter(writableFileFormats());
        m_fileName = QFileDialog::getSaveFileName(parentWidget(), tr("Save %1").arg(documentType()),
                                                  {} /* FIXME: set workdir */, std::move(filters));

        if (m_fileName.isEmpty())
            return QDialog::Rejected;
    }

    if (const auto fileHandler = writeFile(m_fileName); fileHandler->failed()) {
        const auto fileInfo = QFileInfo{fileHandler->fileName()};

        QMessageBox::warning(parentWidget(), tr("Saving failed"),
                             tr("Saving your %1 to <b>\"%2\"</b> has failed.<br><br>%3").
                             arg(documentType(), fileInfo.fileName(), fileHandler->errorString()));

        return QDialog::Rejected;
    } else {
        m_fileName = fileHandler->fileName();
        m_recentFilesMenu->addFileName(m_fileName);
        resetModified();
    }

    return QDialog::Accepted;
}

} // namespace lmrs::widgets
