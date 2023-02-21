#include "recentfilemenu.h"

#include "actionutils.h"

#include <lmrs/core/memory.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <QFileInfo>
#include <QSettings>

namespace lmrs::widgets {

namespace {

auto toStringList(QList<QFileInfo> fileInfoList)
{
    auto stringList = QStringList{};
    stringList.reserve(fileInfoList.size());
    std::transform(fileInfoList.begin(), fileInfoList.end(), std::back_inserter(stringList),
                   [](QFileInfo fileInfo) { return fileInfo.canonicalFilePath(); });
    return stringList;
}

auto toFileInfoList(QStringList fileNameList)
{
    auto fileInfoList = QList<QFileInfo>{};
    fileInfoList.reserve(fileNameList.size());

    for (auto fileName: fileNameList) {
        auto fileInfo = QFileInfo{std::move(fileName)};

        if (fileInfo.exists())
            fileInfoList.emplaceBack(std::move(fileInfo));
    }

    return fileInfoList;
}

} // namespace

class RecentFileMenu::Private : public core::PrivateObject<RecentFileMenu>
{
public:
    using PrivateObject::PrivateObject;

    core::ConstPointer<QAction> m_separatorAction{q()};

    QString m_settingsKey;
    int m_maximumSize = 5;

    void createActions();
    void updateActions();
};

void RecentFileMenu::Private::createActions()
{
    q()->clear();

    for (int i = 0; i < m_maximumSize; ++i) {
        auto action = std::make_unique<QAction>(this);

        connect(action.get(), &QAction::triggered, this, [this, action = action.get()] {
            emit q()->fileSelected(action->data().toString(), RecentFileMenu::QPrivateSignal{});
        });

        q()->addAction(action.release());
    }

    updateActions();
}

void RecentFileMenu::Private::updateActions()
{
    const auto recentFiles = q()->recentFileNames().mid(0, m_maximumSize);

    // initialize used actions
    for (auto i = 0U; i < recentFiles.size(); ++i) {
        const auto fileInfo = QFileInfo{recentFiles.at(i)};
        const auto action = q()->actions().at(i);

        action->setText(u"&%1. %2"_qs.arg(QString::number(i + 1), fileInfo.fileName()));
        action->setData(fileInfo.filePath());
        action->setVisible(true);
    }

    // hide unused actions
    for (auto i = recentFiles.size(); i < q()->actions().count(); ++i)
        q()->actions().at(i)->setVisible(false);

    m_separatorAction->setVisible(!recentFiles.isEmpty());
}

RecentFileMenu::RecentFileMenu(QString settingsKey, QWidget *parent)
    : QMenu{parent}
    , d{new Private{this}}
{
    d->m_settingsKey = std::move(settingsKey);
    d->m_separatorAction->setSeparator(true);
    d->createActions();

    connect(d->m_separatorAction, &QAction::visibleChanged, this, [this] {
        emit hasFilesChanged(hasFiles(), QPrivateSignal{});
    });
}

void RecentFileMenu::addFileName(QString fileName)
{
    addFileInfo(QFileInfo{std::move(fileName)});
}

void RecentFileMenu::addFileInfo(QFileInfo fileInfo)
{
    auto recentFiles = recentFileNames();

    recentFiles.removeAll(fileInfo);
    recentFiles.prepend(std::move(fileInfo));

    while (recentFiles.length() > d->m_maximumSize)
        recentFiles.removeLast();

    QSettings{}.setValue(d->m_settingsKey, toStringList(std::move(recentFiles)));

    d->updateActions();
}

QList<QFileInfo> RecentFileMenu::recentFileNames() const
{
    return toFileInfoList(QSettings{}.value(d->m_settingsKey).toStringList());
}

QAction *RecentFileMenu::separatorAction() const
{
    return d->m_separatorAction;
}

bool RecentFileMenu::hasFiles() const
{
    return d->m_separatorAction->isVisible();
}

QAction *RecentFileMenu::bindMenuAction(QAction *action)
{
    connect(this, &RecentFileMenu::hasFilesChanged, action, &QAction::setVisible);

    action->setMenu(this);
    action->setVisible(hasFiles());

    return action;
}

QAction *RecentFileMenu::bindToolBarAction(QAction *action, QToolBar *toolBar)
{
    const auto updateRecentFilesPopups = [this, action, toolBar](bool hasFiles) {
        if (hasFiles) {
            action->setMenu(this);
            forceMenuButtonMode(toolBar, action);
        } else  {
            action->setMenu(nullptr);
        }
    };

    connect(this, &RecentFileMenu::hasFilesChanged, this, updateRecentFilesPopups);
    updateRecentFilesPopups(hasFiles());

    return action;
}

} // namespace lmrs::widgets
