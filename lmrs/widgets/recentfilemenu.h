#ifndef LMRS_WIDGETS_RECENTFILEMENU_H
#define LMRS_WIDGETS_RECENTFILEMENU_H

#include <QMenu>

class QFileInfo;
class QToolBar;

namespace lmrs::widgets {

class RecentFileMenu : public QMenu
{
    Q_OBJECT

public:
    explicit RecentFileMenu(QString settingsKey, QWidget *parent = nullptr);

    void addFileName(QString fileName);
    void addFileInfo(QFileInfo fileInfo);
    QList<QFileInfo> recentFileNames() const;
    QAction *separatorAction() const;
    bool hasFiles() const;

    QAction *bindMenuAction(QAction *action);
    QAction *bindToolBarAction(QAction *action, QToolBar *toolBar);

signals:
    void fileSelected(QString fileName, QPrivateSignal);
    void hasFilesChanged(bool hasFiles, QPrivateSignal);

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_RECENTFILEMENU_H
