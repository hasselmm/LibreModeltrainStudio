#include "mainwindow.h"

#include "functioneditor.h"
#include "packageexplorer.h"
#include "packagelistmodel.h"
#include "packagesummary.h"
#include "soundsloteditor.h"

#include "metadata.h"
#include "package.h"

#include <lmrs/core/userliterals.h>
#include <lmrs/gui/fontawesome.h>
#include <lmrs/widgets/navigationtoolbar.h>
#include <lmrs/widgets/recentfilemenu.h>

#include <QApplication>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QToolBar>

#include <QDebug>

using namespace lmrs; // FIXME
using namespace widgets;

class MainWindow::Private : public core::PrivateObject<MainWindow>
{
public:
    using PrivateObject::PrivateObject;

    void setupNavigationActions();

    void onFileMenuOpenFileTriggered();

    NavigationToolBar *const navigation = new NavigationToolBar{q()};
    QStackedWidget *const stack = new QStackedWidget{q()};
    RecentFileMenu *const recentFileMenu = new RecentFileMenu{"RecentFiles"_L1, q()};

    FunctionEditor *const functionEditor = new FunctionEditor{q()};
    PackageExplorer *const packageExplorer = new PackageExplorer{q()};
    PackageSummary *const packageSummary = new PackageSummary{q()};
    SoundSlotEditor *const soundSlotEditor = new SoundSlotEditor{q()};

    std::shared_ptr<esu::Package> package;
};

void MainWindow::Private::setupNavigationActions()
{
    const auto notImplemented = new QLabel{tr("This screen has not been implemented yet"), q()};
    notImplemented->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    notImplemented->setEnabled(false);
    notImplemented->setMargin(20);

    const auto addView = [this](QIcon icon, QString text, QWidget *view) {
        navigation->addView(std::move(icon), std::move(text), view);
        stack->addWidget(view);
    };

    addView(icon(gui::fontawesome::fasPlug), tr("&Connect"), notImplemented);
    addView(icon(gui::fontawesome::fasTrain), tr("&Drive"), notImplemented);

    navigation->addSeparator();

    addView(icon(gui::fontawesome::fasCircleInfo), tr("&Info"), packageSummary);
    addView(icon(gui::fontawesome::fasScrewdriverWrench), tr("&Programming"), notImplemented);
    addView(icon(gui::fontawesome::fasListOl), tr("&Functions"), functionEditor);
    addView(icon(gui::fontawesome::fasVolumeHigh), tr("&Sounds"), soundSlotEditor);
    addView(icon(gui::fontawesome::fasFolderTree), tr("&Explorer"), packageExplorer);

    navigation->addSeparator();

    addView(icon(gui::fontawesome::fasBook), tr("&Decoder\nLibrary"), notImplemented);

    navigation->setCurrentView(stack->currentWidget());
    navigation->attachMainWidget(stack);
}

void MainWindow::Private::onFileMenuOpenFileTriggered()
{
    const auto dialog = std::make_unique<QFileDialog>(q());
    dialog->setNameFilter(tr("ESU Lokprogrammer Project (*.esux)"));

    if (const auto documentLocations = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
            !documentLocations.isEmpty())
        dialog->setDirectory(documentLocations.first());

    if (dialog->exec() == QFileDialog::Accepted) {
        const auto fileName = dialog->selectedFiles().first();
        recentFileMenu->addFileName(fileName);
        q()->open(std::move(fileName));
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}
    , d{new Private{this}}
{
    setCentralWidget(d->stack);

    d->setupNavigationActions();
    addToolBar(Qt::LeftToolBarArea, d->navigation);

    const auto fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open..."), QKeySequence::Open, d, &Private::onFileMenuOpenFileTriggered);

    fileMenu->addAction(d->recentFileMenu->separatorAction());
    fileMenu->addActions(d->recentFileMenu->actions());

    connect(d->recentFileMenu, &RecentFileMenu::fileSelected, this, &MainWindow::open);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    const auto viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addActions(d->navigation->actions());

    /*const auto helpMenu = */menuBar()->addMenu(tr("&Help"));

    // finally try to reopen the most recently open file
    if (const auto fileNames = d->recentFileMenu->recentFileNames(); !fileNames.isEmpty())
        open(fileNames.first().absoluteFilePath());
}

void MainWindow::open(QString filePath)
{
    const auto newPackage = std::make_shared<esu::Package>();

    if (!newPackage->read(filePath)) {
        QMessageBox::warning(this, tr("Could not open package"), newPackage->errorString());
        return;
    }

    // store old package to keep it alive until this method is finished
    const auto oldPackage = std::exchange(d->package, newPackage);
    const auto metaData = esu::MetaData::fromPackage(d->package);

    d->packageExplorer->setPackage(d->package);

    d->functionEditor->setMetaData(metaData);
    d->packageSummary->setMetaData(metaData);
    d->soundSlotEditor->setMetaData(metaData);

    setWindowFilePath(filePath);
}
