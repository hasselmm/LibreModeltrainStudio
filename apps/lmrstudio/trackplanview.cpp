#include "trackplanview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>
#include <lmrs/core/fileformat.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/symbolictrackplanmodel.h>
#include <lmrs/core/typetraits.h>

#include <lmrs/gui/fontawesome.h>
#include <lmrs/gui/localization.h>

#include <lmrs/roco/z21appfilesharing.h>

#include <lmrs/widgets/actionutils.h>
#include <lmrs/widgets/documentmanager.h>
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

class TrackPlanView::Private : public core::PrivateObject<TrackPlanView, widgets::DocumentManager, QString>
{
public:
    using PrivateObject::PrivateObject;

    // widgets::DocumentManager interface ------------------------------------------------------------------------------

    QString documentType() const override { return tr("symbolic track plan"); }
    QList<core::FileFormat> readableFileFormats() const override { return core::SymbolicTrackPlanReader::fileFormats(); }
    QList<core::FileFormat> writableFileFormats() const override { return core::SymbolicTrackPlanWriter::fileFormats(); }

    FileHandlerPointer readFile(QString fileName) override;
    FileHandlerPointer writeFile(QString fileName) override;
    void resetModel(QVariant preset) override;

    // model handling --------------------------------------------------------------------------------------------------

    SymbolicTrackPlanModel *model(int index) const;
    SymbolicTrackPlanModel *currentModel() const;

    SymbolicTrackPlanView *view(int index) const;
    SymbolicTrackPlanView *currentView() const;

    SymbolicTrackPlanView *createView(SymbolicTrackPlanModel *model);

    void onFileSharing();

    void onZoomInAction();
    void onZoomOutAction();

    void onCurrentDeviceChanged();
    void onCurrentPageChanged();

    void onListeningChanged(bool isListening);

    void attachCurrentDevice(widgets::SymbolicTrackPlanView *view);
    std::function<void()> makeResetAction(Preset preset);
    static l10n::String displayName(Preset preset);

    core::ConstPointer<l10n::Action> fileNewAction{icon(gui::fontawesome::fasFile),
                LMRS_TR("&New"), LMRS_TR("Reset new symbolic track plan"),
                this, makeResetAction(Preset::Empty)};
    core::ConstPointer<l10n::Action> fileOpenAction{icon(gui::fontawesome::fasFolderOpen),
                LMRS_TR("&Open"), LMRS_TR("&Open symbolic track plan from disk"),
                QKeySequence::Open, this, &DocumentManager::open};

    core::ConstPointer<l10n::Action> fileOpenRecentAction{LMRS_TR("Recent&ly used files"), this};

    core::ConstPointer<l10n::Action> fileSaveAction{icon(gui::fontawesome::fasFloppyDisk),
                LMRS_TR("&Save"), LMRS_TR("Save current symbolic track plan to disk"),
                QKeySequence::Save, this, &DocumentManager::save};
    core::ConstPointer<l10n::Action> fileSaveAsAction{icon(gui::fontawesome::fasFloppyDisk),
                LMRS_TR("Save &as..."), LMRS_TR("Save current symbolic track plan to disk, under new name"),
                QKeySequence::SaveAs, this, &DocumentManager::saveAs};
    core::ConstPointer<l10n::Action> fileSharingAction{icon(gui::fontawesome::fasShareNodes),
                LMRS_TR("S&hare..."), LMRS_TR("Share current symbolic track plan in the local network"),
                this, &Private::onFileSharing};

    core::ConstPointer<QTabWidget> notebook{q()};
    core::ConstPointer<QComboBox> deviceBox{q()}; // FIXME: move up in class hierachy
    core::ConstPointer<QSpinBox> zoomBox{q()};

    QHash<ActionCategory, QActionGroup *> actionGroups;

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
        resetWithModel(QVariant::fromValue(preset));
    };
}

l10n::String TrackPlanView::Private::displayName(Preset preset)
{
    switch (preset) {
    case Preset::Empty:
        return LMRS_TR("Empty");
    case Preset::SymbolGallery:
        return LMRS_TR("Symbol Gallery");
    case Preset::Loops:
        return LMRS_TR("Various Loops");
    case Preset::Roofs:
        return LMRS_TR("Platforms and Roofs");
    };

    Q_UNREACHABLE();
}

template<int index>
QString addNumericMnemonic(QString &text)
{
    if (index < 10)
        return "&%1. %2"_L1.arg(QString::number(index + 1), text);

    return text;
}

auto numericMnemonicFunction(int index)
{
    switch (index) {
    case 0: return &addNumericMnemonic<0>;
    case 1: return &addNumericMnemonic<1>;
    case 2: return &addNumericMnemonic<2>;
    case 3: return &addNumericMnemonic<3>;
    case 4: return &addNumericMnemonic<4>;
    case 5: return &addNumericMnemonic<5>;
    case 6: return &addNumericMnemonic<6>;
    case 7: return &addNumericMnemonic<7>;
    case 8: return &addNumericMnemonic<8>;
    case 9: return &addNumericMnemonic<9>;
    }

    return &addNumericMnemonic<10>;
}

TrackPlanView::TrackPlanView(QAbstractItemModel *model, QWidget *parent)
    : MainWindowView{parent}
    , d{new Private{"TrackPlanView"_L1, this}}
{
    d->layout = std::make_unique<core::SymbolicTrackPlanLayout>();
    d->layout->pages.emplace_back(std::make_unique<SymbolicTrackPlanModel>());
    d->createView(d->layout->pages.front().get());

    // FIXME: make this method pretty
    const auto newPageAction = new l10n::Action{icon(gui::fontawesome::fasPlus), LMRS_TR("New page"), this};
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
    const auto fileOpenToolBarAction = widgets::createProxyAction(d->fileOpenAction.get());

    toolBar->addAction(d->fileNewAction);
    toolBar->addAction(fileOpenToolBarAction);
    toolBar->addAction(d->fileSaveAction);
    toolBar->addAction(d->fileSharingAction);

    widgets::forceMenuButtonMode(toolBar, d->fileNewAction);
    d->recentFilesMenu()->bindMenuAction(d->fileOpenRecentAction);
    d->recentFilesMenu()->bindToolBarAction(fileOpenToolBarAction, toolBar);

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
        auto text = l10n::String(Private::displayName(preset.value()), numericMnemonicFunction(preset.index()));

        const auto action = new l10n::Action{std::move(text), d, d->makeResetAction(preset.value())};
        d->fileNewAction->menu()->addAction(action);

        if (preset == Preset::Empty)
            action->setShortcut(Qt::ControlModifier | Qt::Key_N);
    }

    // -----------------------------------------------------------------------------------------------------------------

    connect(d, &Private::modifiedChanged, d->fileSaveAction, &QAction::setEnabled);
    d->fileSaveAction->setEnabled(d->isModified());

    connectDocumentManager(d);

    // -----------------------------------------------------------------------------------------------------------------

    d->actionGroups[ActionCategory::FileNew] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileNew]->addAction(d->fileNewAction);

    d->actionGroups[ActionCategory::FileOpen] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileOpen]->addAction(d->fileOpenAction);
    d->actionGroups[ActionCategory::FileOpen]->addAction(d->fileOpenRecentAction);

    d->actionGroups[ActionCategory::FileSave] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileSave]->addAction(d->fileSaveAction);
    d->actionGroups[ActionCategory::FileSave]->addAction(d->fileSaveAsAction);
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

QString TrackPlanView::fileName() const
{
    return d->fileName();
}

bool TrackPlanView::isModified() const
{
    return d->isModified();
}

bool TrackPlanView::open(QString newFileName)
{
    return d->readFile(std::move(newFileName))->succeeded();
}

TrackPlanView::Private::FileHandlerPointer TrackPlanView::Private::readFile(QString fileName)
{
    auto reader = core::SymbolicTrackPlanReader::fromFile(std::move(fileName));

    if (auto newLayout = reader->read()) {
        qInfo() << newLayout->name << newLayout->pages.size();

        notebook->clear();
        layout = std::move(newLayout);

        for (const auto &page: layout->pages)
            createView(page.get());
    }

    return reader;
}

TrackPlanView::Private::FileHandlerPointer TrackPlanView::Private::writeFile(QString fileName)
{
    auto writer = core::SymbolicTrackPlanWriter::fromFile(std::move(fileName));
    const auto succeeded = writer->write(layout.get());
    Q_ASSERT(succeeded == writer->succeeded());
    return writer;
}

void TrackPlanView::Private::resetModel(QVariant model)
{
    if (const auto preset = core::get_if<Preset>(std::move(model)))
        if (const auto model = currentModel())
            model->reset(preset.value());
}

} // namespace studio::lmrs
