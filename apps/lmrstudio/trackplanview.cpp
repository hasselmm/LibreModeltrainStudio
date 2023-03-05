#include "deviceconnectionview.h"
#include "trackplanview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>
#include <lmrs/core/fileformat.h>
#include <lmrs/core/logging.h>
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

template<int index>
QString addNumericMnemonic(QString &text)
{
    if (index < 10)
        return "&%1. %2"_L1.arg(QString::number(index + 1), text);

    return text;
}

constexpr auto numericMnemonicFunction(int index)
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

    void onFileSharingRequested();
    void onCurrentPageChanged();
    void onListeningChanged(bool isListening);

    void updateControls(core::Device *newDevice);
    void attachCurrentDevice(widgets::SymbolicTrackPlanView *view);
    std::function<void()> makeResetAction(Preset preset);
    static l10n::String displayName(Preset preset);

    core::ConstPointer<l10n::Action> fileNewAction{icon(gui::fontawesome::fasFile),
                LMRS_TR("&New"), LMRS_TR("Create new symbolic track plan"),
                this, makeResetAction(Preset::Empty)};
    core::ConstPointer<l10n::Action> fileOpenAction{icon(gui::fontawesome::fasFolderOpen),
                LMRS_TR("&Open"), LMRS_TR("&Open symbolic track plan from disk"),
                QKeySequence::Open, this, &DocumentManager::open};

    core::ConstPointer<l10n::Action> fileOpenRecentAction{LMRS_TR("Recent&ly used files"), this};

    core::ConstPointer<l10n::Action> fileSaveAction{icon(gui::fontawesome::fasFloppyDisk),
                LMRS_TR("&Save"), LMRS_TR("Save current symbolic track plan to disk"),
                QKeySequence::Save, this, &DocumentManager::save};
    core::ConstPointer<l10n::Action> fileSaveAsAction{
                LMRS_TR("Save &as..."), LMRS_TR("Save current symbolic track plan to disk, under new name"),
                QKeySequence::SaveAs, this, &DocumentManager::saveAs};
    core::ConstPointer<l10n::Action> fileSharingAction{icon(gui::fontawesome::fasShareNodes),
                LMRS_TR("S&hare..."), LMRS_TR("Share current symbolic track plan in the local network"),
                this, &Private::onFileSharingRequested};

    core::ConstPointer<QTabWidget> notebook{q()};
    core::ConstPointer<widgets::ZoomActionGroup> zoomActions{this};
    QHash<ActionCategory, QActionGroup *> actionGroups;

    QPointer<FileSharing> fileSharing;
    QPointer<core::Device> currentDevice;

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
            zoomActions->setCurrentZoom(tileSize);
    });

    return view;
}

void TrackPlanView::Private::onFileSharingRequested()
{
    LMRS_UNIMPLEMENTED();
}

void TrackPlanView::Private::onListeningChanged(bool isListening)
{
    fileSharingAction->setEnabled(isListening);
}

void TrackPlanView::Private::attachCurrentDevice(SymbolicTrackPlanView *view)
{
    const auto accessoryControl = currentDevice ? currentDevice->accessoryControl() : nullptr;
    const auto detectorControl = currentDevice ? currentDevice->detectorControl() : nullptr;

    view->setAccessoryControl(accessoryControl);
    view->setDetectorControl(detectorControl);
}

void TrackPlanView::Private::updateControls(core::Device *newDevice)
{
    if (const auto oldDevice = std::exchange(currentDevice, newDevice); oldDevice != newDevice) {
        for (auto i = 0; i < notebook->count(); ++i)
            attachCurrentDevice(view(i));
    }
}

void TrackPlanView::Private::onCurrentPageChanged()
{
    if (const auto view = currentView())
        zoomActions->setCurrentZoom(view->tileSize());
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

TrackPlanView::TrackPlanView(QWidget *parent)
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

    connect(d->zoomActions, &widgets::ZoomActionGroup::currentZoomChanged, this, [this](auto tileSize) {
        if (const auto view = d->currentView())
            view->setTileSize(tileSize);
    });

    d->recentFilesMenu()->bindMenuAction(d->fileOpenRecentAction);
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

    for (const auto actionList = d->zoomActions->actions(); const auto action: actionList)
        d->actionGroups[ActionCategory::View]->addAction(action);

    const auto layout = new QVBoxLayout{this};

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

QList<QActionGroup *> TrackPlanView::actionGroups(ActionCategory category) const
{
    return {d->actionGroups[category]};
}

TrackPlanView::FileState TrackPlanView::fileState() const
{
    return d->isModified() ? FileState::Modified : FileState::Saved;
}

QString TrackPlanView::fileName() const
{
    return d->fileName();
}

void TrackPlanView::updateControls(core::Device *newDevice)
{
    d->updateControls(newDevice);
}

DeviceFilter TrackPlanView::deviceFilter() const
{
    return acceptAny<
            DeviceFilter::Require<core::AccessoryControl>,
            DeviceFilter::Require<core::DetectorControl>>();
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
