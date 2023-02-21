#include "trackplanview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>
#include <lmrs/core/fileformat.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/symbolictrackplanmodel.h>
#include <lmrs/core/typetraits.h>

#include <lmrs/gui/fontawesome.h>

#include <lmrs/roco/z21appfilesharing.h>

#include <lmrs/widgets/actionutils.h>
#include <lmrs/widgets/recentfilemenu.h>
#include <lmrs/widgets/symbolictrackplanview.h>

#include <QActionGroup>
#include <QBoxLayout>
#include <QComboBox>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>

namespace lmrs::studio {

namespace {

using core::SymbolicTrackPlanModel;
using roco::z21app::FileSharing;
using widgets::SymbolicTrackPlanView;

using Preset = SymbolicTrackPlanModel::Preset;

} // namespace

class TrackPlanView::Private : public core::PrivateObject<TrackPlanView>
{
public:
    using PrivateObject::PrivateObject;

    SymbolicTrackPlanModel *model(int index) const;
    SymbolicTrackPlanModel *currentModel() const;

    SymbolicTrackPlanView *view(int index) const;
    SymbolicTrackPlanView *currentView() const;

    SymbolicTrackPlanView *createView(SymbolicTrackPlanModel *model);

    void onFileOpen();
    void onFileSave();
    void onFileSharing();

    void onZoomInAction();
    void onZoomOutAction();

    void onCurrentDeviceChanged();
    void onCurrentPageChanged();

    void onListeningChanged(bool isListening);

    void attachCurrentDevice(widgets::SymbolicTrackPlanView *view);
    std::function<void()> makeResetAction(Preset preset);
    static QString displayName(Preset preset);

    bool open(QString fileName);

    core::ConstPointer<QAction> fileNewAction = q()->addAction(icon(gui::fontawesome::fasFile), tr("&Reset Track Plan"),
                                                               this, makeResetAction(Preset::Empty));
    core::ConstPointer<QAction> fileOpenAction = q()->addAction(icon(gui::fontawesome::fasFolderOpen), tr("&Open Track Plan"),
                                                                QKeySequence::Open, this, &Private::onFileOpen);
    core::ConstPointer<QAction> fileOpenRecentAction = q()->addAction(tr("&Recent Track Plans..."));
    core::ConstPointer<QAction> fileSaveAction = q()->addAction(icon(gui::fontawesome::fasFloppyDisk), tr("&Save Track Plan"),
                                                                QKeySequence::Save, this, &Private::onFileSave);
    core::ConstPointer<QAction> fileSharingAction = q()->addAction(icon(gui::fontawesome::fasShareNodes), tr("S&hare Track Plan"),
                                                                   this, &Private::onFileSharing);

    core::ConstPointer<QTabWidget> notebook{q()};
    core::ConstPointer<QComboBox> deviceBox{q()}; // FIXME: move up in class hierachy
    core::ConstPointer<QSpinBox> zoomBox{q()};

    QHash<ActionCategory, QActionGroup *> actionGroups;
    core::ConstPointer<widgets::RecentFileMenu> recentFilesMenu{"TrackPlanView/RecentFiles"_L1, q()};

    QPointer<FileSharing> fileSharing;

    std::unique_ptr<core::SymbolicTrackPlanLayout> layout;
};

SymbolicTrackPlanModel *TrackPlanView::Private::model(int index) const
{
    if (layout)
        return layout->pages[static_cast<size_t>(index)].get();

    return nullptr;
}

SymbolicTrackPlanModel *TrackPlanView::Private::currentModel() const
{
    return model(notebook->currentIndex());
}

SymbolicTrackPlanView *TrackPlanView::Private::view(int index) const
{
    if (const auto scroller = core::checked_cast<QScrollArea *>(notebook->widget(index)))
        if (const auto view = core::checked_cast<SymbolicTrackPlanView *>(scroller->widget()))
            return view;

    return {};
}

SymbolicTrackPlanView *TrackPlanView::Private::currentView() const
{
    return view(notebook->currentIndex());
}

SymbolicTrackPlanView *TrackPlanView::Private::createView(SymbolicTrackPlanModel *model)
{
    const auto view = new SymbolicTrackPlanView{q()};
    attachCurrentDevice(view);
    view->setModel(model);

    const auto scroller = new QScrollArea{notebook};
    scroller->setWidgetResizable(true);
    scroller->setWidget(view);

    notebook->addTab(scroller, core::coalesce(model->title(), tr("untitled")));

    connect(model, &SymbolicTrackPlanModel::titleChanged, this, [this, view](QString title) {
        notebook->setTabText(notebook->indexOf(view), core::coalesce(std::move(title), tr("untitled")));
    });


    connect(view, &SymbolicTrackPlanView::tileSizeChanged, this, [this, view](int tileSize) {
        if (view == currentView())
            zoomBox->setValue(tileSize);
    });

    return view;
}

void TrackPlanView::Private::onFileOpen()
{
    // FIXME: Use proper documents folder from QDesktopServices, from QSettings
    auto filters = core::FileFormat::openFileDialogFilter(core::SymbolicTrackPlanReader::fileFormats());
    auto fileName = QFileDialog::getOpenFileName(q(), tr("Open Track Plan"), {}, std::move(filters));

    if (!fileName.isEmpty())
        q()->open(std::move(fileName));
}

void TrackPlanView::Private::onFileSave()
{
    // FIXME: Use proper documents folder from QDesktopServices, from QSettings
    auto filters = core::FileFormat::saveFileDialogFilter(core::SymbolicTrackPlanWriter::fileFormats());
    auto fileName = QFileDialog::getSaveFileName(q(), tr("Save Track Plan"), {}, std::move(filters));

    if (!fileName.isEmpty()) {
        const auto writer = core::SymbolicTrackPlanWriter::fromFile(std::move(fileName));

        if (!writer->write(std::move(layout.get()))) {
            QMessageBox::critical(q(), tr("Could not write track layout"),
                                  tr("<p>Could not write this track layout to <b>%1</b>.</p><p>%2</p>").
                                  arg(QFileInfo{writer->fileName()}.fileName(), writer->errorString()));
        }
    }
}

void TrackPlanView::Private::onFileSharing()
{
    Q_UNIMPLEMENTED();
}

void TrackPlanView::Private::onZoomInAction()
{
    if (const auto view = currentView())
        view->setTileSize(qMin(25 * (1 << qFloor(log(view->tileSize() / 25.0) / log(2) + 1)), 400));
}

void TrackPlanView::Private::onZoomOutAction()
{
    if (const auto view = currentView())
        view->setTileSize(qMax(25 * (1 << qFloor(log((view->tileSize() - 1) / 25.0) / log(2))), 25));
}

void TrackPlanView::Private::onListeningChanged(bool isListening)
{
    fileSharingAction->setEnabled(isListening);
}

void TrackPlanView::Private::attachCurrentDevice(SymbolicTrackPlanView *view)
{
    const auto device = qvariant_cast<core::Device * >(deviceBox->currentData());
    const auto accessoryControl = device ? device->accessoryControl() : nullptr;
    const auto detectorControl = device ? device->detectorControl() : nullptr;

    view->setAccessoryControl(accessoryControl);
    view->setDetectorControl(detectorControl);
}

void TrackPlanView::Private::onCurrentDeviceChanged()
{
    for (auto i = 0; i < notebook->count(); ++i)
        attachCurrentDevice(view(i));
}

void TrackPlanView::Private::onCurrentPageChanged()
{
    if (const auto view = currentView())
        zoomBox->setValue(view->tileSize());
}

std::function<void()> TrackPlanView::Private::makeResetAction(Preset preset)
{
    return [this, preset] {
        if (const auto model = currentModel())
            model->reset(preset);
    };
}

QString TrackPlanView::Private::displayName(Preset preset)
{
    switch (preset) {
    case Preset::Empty:
        return tr("Empty");
    case Preset::SymbolGallery:
        return tr("Symbol Gallery");
    case Preset::Loops:
        return tr("Various Loops");
    case Preset::Roofs:
        return tr("Platforms and Roofs");
    };

    Q_UNREACHABLE();
    return QString::number(core::value(preset));
}

TrackPlanView::TrackPlanView(QAbstractItemModel *model, QWidget *parent)
    : MainWindowView{parent}
    , d{new Private{this}}
{
    d->layout = std::make_unique<core::SymbolicTrackPlanLayout>();
    d->layout->pages.emplace_back(std::make_unique<SymbolicTrackPlanModel>());
    d->createView(d->layout->pages.front().get());

    // FIXME: make this method pretty
    const auto newPageAction = addAction(icon(gui::fontawesome::fasPlus), tr("New page"));
    const auto newPageButton = new QPushButton{this};
    widgets::bindAction(newPageButton, newPageAction);
    d->notebook->setCornerWidget(newPageButton, Qt::Corner::TopRightCorner);
    connect(d->notebook, &QTabWidget::currentChanged, d, &Private::onCurrentPageChanged);

    d->zoomBox->setRange(25, 400);

    connect(d->zoomBox, &QSpinBox::valueChanged, this, [this](auto tileSize) {
        if (const auto view = d->currentView())
            view->setTileSize(tileSize);
    });

    const auto toolBar = new QToolBar{this};
    const auto fileOpenToolBarAction = widgets::createProxyAction(d->fileOpenAction);

    toolBar->addAction(d->fileNewAction);
    toolBar->addAction(fileOpenToolBarAction);
    toolBar->addAction(d->fileSaveAction);
    toolBar->addAction(d->fileSharingAction);

    widgets::forceMenuButtonMode(toolBar, d->fileNewAction);
    d->recentFilesMenu->bindMenuAction(d->fileOpenRecentAction);
    d->recentFilesMenu->bindToolBarAction(fileOpenToolBarAction, toolBar);

    toolBar->addSeparator();
    const auto zoomInAction = toolBar->addAction(icon(gui::fontawesome::fasMagnifyingGlassPlus),
                                                 tr("Zoom in"), d, &Private::onZoomInAction);
    const auto zoomOutAction = toolBar->addAction(icon(gui::fontawesome::fasMagnifyingGlassMinus),
                                                  tr("Zoom out"), d, &Private::onZoomOutAction);
    const auto zoomBoxAction = toolBar->addWidget(d->zoomBox);
    zoomBoxAction->setPriority(QAction::LowPriority);

    // FIXME: make this generic
    const auto spacer = new QWidget{this};
    const auto spacerLayout = new QHBoxLayout{spacer};
    spacerLayout->addStretch(1);

    // FIXME: make this generic
    d->deviceBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d->deviceBox->setModel(model);

    toolBar->addWidget(spacer);
    const auto deviceBoxAction = toolBar->addWidget(d->deviceBox);

    const auto hideEmptyDeviceCombo = [deviceBoxAction, model] {
        deviceBoxAction->setVisible(model->rowCount() > 0);
    };

    connect(model, &QAbstractItemModel::modelReset, this, hideEmptyDeviceCombo);
    connect(model, &QAbstractItemModel::rowsInserted, this, hideEmptyDeviceCombo);
    connect(model, &QAbstractItemModel::rowsRemoved, this, hideEmptyDeviceCombo);

    hideEmptyDeviceCombo();

    connect(d->deviceBox, &QComboBox::currentIndexChanged, d, &Private::onCurrentDeviceChanged);

    // FIXME: make this generic
    const auto dettachAction = toolBar->addAction(icon(gui::fontawesome::fasArrowUpRightFromSquare),
                                                  "Detach from main window"_L1, this, [this] {
        setParent(nullptr);
        setWindowTitle(tr("Track Plan"));
        setWindowFilePath(tr("Hello.txt"));
        core::checked_cast<QAction *>(sender())->setVisible(false);
        show();
    });

    dettachAction->setCheckable(true);
    dettachAction->setPriority(QAction::HighPriority);

    d->fileNewAction->setMenu(new QMenu{this});

    for (const auto preset: QMetaTypeId<Preset>{}) {
        auto text = '&'_L1 + QString::number(preset.index() + 1) + ". "_L1 + Private::displayName(preset.value());
        const auto action = d->fileNewAction->menu()->addAction(std::move(text), d, d->makeResetAction(preset.value()));

        if (preset == Preset::Empty)
            action->setShortcut(Qt::ControlModifier | Qt::Key_N);
    }

    connect(d->recentFilesMenu, &widgets::RecentFileMenu::fileSelected, this, &TrackPlanView::open);

    d->actionGroups[ActionCategory::FileNew] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileNew]->addAction(d->fileNewAction);

    d->actionGroups[ActionCategory::FileOpen] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileOpen]->addAction(d->fileOpenAction);
    d->actionGroups[ActionCategory::FileOpen]->addAction(d->fileOpenRecentAction);

    d->actionGroups[ActionCategory::FileSave] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileSave]->addAction(d->fileSaveAction);
    d->actionGroups[ActionCategory::FileSave]->addAction(d->fileSharingAction);

    d->actionGroups[ActionCategory::EditCreate] = new QActionGroup{this};
    d->actionGroups[ActionCategory::EditCreate]->addAction(newPageAction);

    d->actionGroups[ActionCategory::View] = new QActionGroup{this};
    d->actionGroups[ActionCategory::View]->addAction(zoomInAction);
    d->actionGroups[ActionCategory::View]->addAction(zoomOutAction);

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(toolBar);
    layout->addWidget(d->notebook);

    d->onListeningChanged(false);
}

void TrackPlanView::setFileSharing(FileSharing *newFileSharing)
{
    if (const auto oldFileSharing = std::exchange(d->fileSharing, newFileSharing);
            oldFileSharing != newFileSharing) {
        if (oldFileSharing)
            oldFileSharing->disconnect(this);

        if (newFileSharing) {
            connect(newFileSharing, &FileSharing::isListeningChanged, d, &Private::onListeningChanged);
            d->onListeningChanged(newFileSharing->isListening());
        }
    }
}

FileSharing *TrackPlanView::fileSharing() const
{
    return d->fileSharing;
}

QActionGroup *TrackPlanView::actionGroup(ActionCategory category) const
{
    return d->actionGroups[category];
}

bool TrackPlanView::Private::open(QString fileName)
{
    if (fileName.isEmpty()) // FIXME staticassert
        return false;

    const auto reader = core::SymbolicTrackPlanReader::fromFile(std::move(fileName));

    if (auto newLayout = reader->read(); !newLayout) {
        QMessageBox::critical(q(), tr("Could not read track layout"),
                              tr("<p>Could not read a track layout from <b>%1</b>.</p><p>%2</p>").
                              arg(QFileInfo{reader->fileName()}.fileName(), reader->errorString()));

        return false;
    } else {
        qInfo() << newLayout->name << newLayout->pages.size();

        notebook->clear();
        recentFilesMenu->addFileName(reader->fileName());
        layout = std::move(newLayout);

        for (const auto &page: layout->pages)
            createView(page.get());

        return true;
    }
}

bool TrackPlanView::open(QString fileName)
{
    return d->open(std::move(fileName));
}

} // namespace studio::lmrs