#include "mainwindow.h"

#include "accessorycontrolview.h"
#include "automationview.h"
#include "debugview.h"
#include "decoderdatabaseview.h"
#include "deviceconnectionview.h"
#include "functionmappingview.h"
#include "multideviceview.h"
#include "speedmeterview.h"
#include "trackplanview.h"
#include "variableeditorview.h"
#include "vehiclecontrolview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/device.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/gui/fontawesome.h>

#include <lmrs/roco/z21appfilesharing.h>

#include <lmrs/widgets/navigationtoolbar.h>
#include <lmrs/widgets/statusbar.h>

#include <QActionGroup>
#include <QApplication>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>

namespace lmrs::studio {

namespace {

using namespace roco::z21app;
using namespace widgets;

constexpr auto s_settings_fileSharingEnabled = "MainWindow/FileSharingEnabled"_L1;
constexpr auto s_settings_lastVehicleAddress = "MainWindow/VehicleAddress"_L1;

enum class PrettyFileSizeMode {
    AppendUnit,
    SkipUnit,
};

auto prettyFileSize(qint64 fileSize, PrettyFileSizeMode mode = PrettyFileSizeMode::AppendUnit) // FIXME: do it properly
{
    auto result = QLocale{}.toString(fileSize);

    if (mode == PrettyFileSizeMode::AppendUnit)
        result += " Bytes"_L1;

    return result;
}

auto checkable(auto action)
{
    action->setCheckable(true);
    return action;
}

} // namespace

class MainWindow::Private : public core::PrivateObject<MainWindow>
{
    Q_OBJECT

public:
    using PrivateObject::PrivateObject;

    auto deviceState() const { return currentDevice ? currentDevice->state() : core::Device::State::Disconnected; }
    auto powerControl() const { return deviceState() == core::Device::State::Connected ? currentDevice->powerControl() : nullptr; }
    auto speedMeter() const { return deviceState() == core::Device::State::Connected ? currentDevice->speedMeterControl() : nullptr; }
    auto vehicleControl() const { return deviceState() == core::Device::State::Connected ? currentDevice->vehicleControl() : nullptr; }
    auto variableControl() const { return deviceState() == core::Device::State::Connected ? currentDevice->variableControl() : nullptr; }

    void setupActions();
    void setupViews();
    void setupMenuBar();
    void setupStatusBar();
    void setupFileReceiver();

    void restoreSettings();

signals:
    void deviceInfoChanged(QList<lmrs::core::DeviceInfo> changedIds);

public:
    void mergeActions(MainWindowView *view);

    void onCurrentViewChanged();
    void onFileNameChanged();
    void onModifiedChanged();

    void onCurrentDeviceChanged(core::Device *newDevice);
    void onDeviceStateChanged(core::Device::State state);
    void onCurrentVehicleChanged(core::dcc::VehicleAddress address);
    void onPowerStateChanged();

    void onFileTransferRequested(FileTransferSession *session);
    void onFileSharingProgress(FileTransferSession *session, qint64 bytesReceived, qint64 totalBytes);
    void onFileSharingFinished(FileTransferSession *session);
    void onFileSharingEnabled();

    QPointer<core::Device> currentDevice;

    core::ConstPointer<QStackedWidget> stack{q()};
    core::ConstPointer<NavigationToolBar> navigation{q()};
    core::ConstPointer<DeviceConnectionView> devicesView{q()};
    core::ConstPointer<MultiDeviceView<VehicleControlView>> vehicleControlView{devicesView->model<core::VehicleControl>(), q()};
    core::ConstPointer<MultiDeviceView<AccessoryControlView>> accessoryControlView{devicesView->model<core::AccessoryControl>(), q()};
    core::ConstPointer<MultiDeviceView<SpeedMeterView>> speedMeterView{devicesView->model<core::SpeedMeterControl>(), q()};
    core::ConstPointer<MultiDeviceView<VariableEditorView>> variableEditorView{devicesView->model<core::VariableControl>(), q()};
// FIXME:    core::ConstPointer<MultiDeviceView<FunctionMappingView>> functionMappingView{devicesView->model<core::VariableControl>(), mainWindow()};
    core::ConstPointer<FunctionMappingView> functionMappingView{devicesView->model<core::VariableControl>(), q()};
    core::ConstPointer<MultiDeviceView<DebugView>> debugView{devicesView->model<core::DebugControl>(), q()};
    core::ConstPointer<DecoderDatabaseView> decoderDatabaseView{q()};
    core::ConstPointer<TrackPlanView> trackPlanView{devicesView->model<core::AccessoryControl>(), q()}; // FIXME: we also handle response modules
    core::ConstPointer<AutomationView> automationView{q()}; // FIXME: we also handle response modules
    core::ConstPointer<StatusBar> statusBar{q()};

    core::ConstPointer<QMenu> deviceMenu{tr("&Device"), q()->menuBar()};
    core::ConstPointer<QActionGroup> powerActionGroup{this};
    core::ConstPointer<QAction> enablePowerAction{icon(gui::fontawesome::fasPlugCircleBolt), tr("&Enable track power"), powerActionGroup};
    core::ConstPointer<QAction> disablePowerAction{icon(gui::fontawesome::fasPlugCircleMinus), tr("&Disable track power"), powerActionGroup};
    core::ConstPointer<QAction> emergencyStopAction{icon(gui::fontawesome::fasPlugCircleXmark), tr("Emergency &stop"), powerActionGroup};
    core::ConstPointer<QAction> shortcircutAction{icon(gui::fontawesome::fasPlugCircleExclamation), tr("Shortcircut"), powerActionGroup};
    core::ConstPointer<QAction> serviceModeAction{icon(gui::fontawesome::fasScrewdriverWrench), tr("Service mode"), powerActionGroup};

    core::ConstPointer<QAction> fileSharingEnabledAction{checkable(q()->addAction(tr("&Z21 File Sharing"), this, &Private::onFileSharingEnabled))};

    core::ConstPointer<FileSharing> fileSharing{this};

    struct ActionCategory
    {
        using Type = MainWindowView::ActionCategory;

        QPointer<QAction> placeholder;
        QHash<QWidget *, QList<QPointer<QActionGroup>>> actionGroups;
    };

    QHash<ActionCategory::Type, ActionCategory> actionCategories;

    core::ConstPointer<QMenu> fileMenu{q()->menuBar()->addMenu(tr("&File"))};
    core::ConstPointer<QMenu> editMenu{q()->menuBar()->addMenu(tr("&Edit"))};
    core::ConstPointer<QMenu> viewMenu{q()->menuBar()->addMenu(tr("&View"))};
};

void MainWindow::Private::setupActions()
{
    for (const auto action: powerActionGroup->actions())
        action->setCheckable(true);

    powerActionGroup->setExclusive(true);
    emergencyStopAction->setEnabled(false);
    shortcircutAction->setEnabled(false);
    serviceModeAction->setEnabled(false);

    connect(disablePowerAction, &QAction::toggled, this, [this](auto toggled) {
        if (const auto control = powerControl(); control && toggled)
            control->disableTrackPower(core::retryOnError);
    });

    connect(enablePowerAction, &QAction::toggled, this, [this](auto toggled) {
        if (const auto control = powerControl(); control && toggled)
            control->enableTrackPower(core::retryOnError);
    });
}

void MainWindow::Private::setupViews()
{
    connect(stack, &QStackedWidget::currentChanged, this, &Private::onCurrentViewChanged);

    const auto notImplemented = new QLabel{tr("This screen has not been implemented yet"), q()};
    notImplemented->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    notImplemented->setEnabled(false);
    notImplemented->setMargin(20);

    const auto addView = [this](QIcon icon, QString text, QWidget *view) {
        if (const auto mainWindowView = dynamic_cast<MainWindowView *>(view)) {
            connect(mainWindowView, &MainWindowView::fileNameChanged, this, &Private::onFileNameChanged);
            connect(mainWindowView, &MainWindowView::modifiedChanged, this, &Private::onModifiedChanged);
            mergeActions(mainWindowView);
        }

        navigation->addView(std::move(icon), std::move(text), view);
        stack->addWidget(view);
    };

    addView(icon(gui::fontawesome::fasPlug), tr("&Connect"), devicesView);
    addView(icon(gui::fontawesome::fasTrain), tr("&Drive"), vehicleControlView);
    addView(icon(gui::fontawesome::fasTrafficLight), tr("S&ignal Box"), accessoryControlView);
    addView(icon(gui::fontawesome::fasGaugeHigh), tr("Speed&meter"), speedMeterView);
    addView(icon(gui::fontawesome::fasScrewdriverWrench), tr("&Programming"), variableEditorView);
    addView(icon(gui::fontawesome::fasBug), tr("De&bug"), debugView);

    navigation->addSeparator();

    addView(icon(gui::fontawesome::fasCodeBranch), tr("&Track Plan"), trackPlanView);
    addView(icon(gui::fontawesome::fasRobot), tr("&Automation"), automationView);

    navigation->addSeparator();

    addView(icon(gui::fontawesome::fasCircleInfo), tr("&Summary"), notImplemented);
    addView(icon(gui::fontawesome::fasTableList), tr("&Functions"), functionMappingView);
    addView(icon(gui::fontawesome::fasVolumeHigh), tr("&Sounds"), notImplemented);
    addView(icon(gui::fontawesome::fasFolderTree), tr("&Explorer"), notImplemented);

    navigation->addSeparator();

    addView(icon(gui::fontawesome::fasBook), tr("&Decoder\nLibrary"), decoderDatabaseView);

    navigation->setCurrentView(stack->currentWidget());
    navigation->attachMainWidget(stack);

    viewMenu->addActions(navigation->actions());

    connect(devicesView, &DeviceConnectionView::currentDeviceChanged, this, &Private::onCurrentDeviceChanged); // FIXME: this has to be done differently

// FIXME    connect(vehicleControlView, &VehicleControlView::currentVehicleChanged, this, &Private::onCurrentVehicleChanged);
// FIXME    connect(variableEditorView, &VariableEditorView::currentVehicleChanged, this, &Private::onCurrentVehicleChanged);

    onCurrentVehicleChanged(QSettings{}.value(s_settings_lastVehicleAddress, 3).
                            value<core::dcc::VehicleAddress::value_type>());
}

void MainWindow::Private::setupMenuBar()
{
    // -----------------------------------------------------------------------------------------------------------------

    actionCategories[ActionCategory::Type::FileNew].placeholder = fileMenu->addSeparator();
    actionCategories[ActionCategory::Type::FileOpen].placeholder = fileMenu->addSeparator();
    actionCategories[ActionCategory::Type::FileSave].placeholder = fileMenu->addSeparator();

    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    // -----------------------------------------------------------------------------------------------------------------

    actionCategories[ActionCategory::Type::EditCreate].placeholder = editMenu->addSeparator();
    actionCategories[ActionCategory::Type::EditClipboard].placeholder = editMenu->addSeparator();

    // -----------------------------------------------------------------------------------------------------------------

    actionCategories[ActionCategory::Type::View].placeholder = viewMenu->addSeparator();

    q()->menuBar()->addMenu(deviceMenu);
    deviceMenu->addActions(powerActionGroup->actions());
    deviceMenu->addSeparator();
    deviceMenu->addAction(tr("Dis&connect"), devicesView.get(), &DeviceConnectionView::disconnectFromDevice);

    const auto settingsMenu = q()->menuBar()->addMenu(tr("&Settings")); // ---------------------------------------------

    settingsMenu->addAction(fileSharingEnabledAction);

    const auto helpMenu = q()->menuBar()->addMenu(tr("&Help")); // -----------------------------------------------------

    helpMenu->addAction(tr("&About %1...").arg(qApp->applicationDisplayName()), this, [this] {
        auto title = tr("About %1").arg(qApp->applicationDisplayName());
        auto text = tr("<p><b>%1 %2</b></p><p>Visit <a href=\"%3/\">%4</a> for information.</p>").
                arg(qApp->applicationDisplayName(), qApp->applicationVersion(), LMRS_HOMEPAGE_URL,
                    QString{LMRS_HOMEPAGE_URL}.replace("https://"_L1, QString{}));

        QMessageBox::about(q(), std::move(title), std::move(text));
    });

    helpMenu->addAction(tr("&About Qt..."), this, [this] {
        QMessageBox::aboutQt(q());
    });
}

void MainWindow::Private::setupStatusBar()
{
    auto createStatusLabel = [this](core::DeviceInfo id, QList<QAction *> actions = {}) {
        const auto label = new QLabel{statusBar};
        label->addActions(std::move(actions));
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setContextMenuPolicy(Qt::ContextMenuPolicy::ActionsContextMenu);
        label->setMinimumWidth(50);
        label->hide();

        const auto updateDeviceInfo = [this, id, label](QList<core::DeviceInfo> changedIds) {
            if (!changedIds.contains(id))
                return;

            if (currentDevice) {
                label->setText(currentDevice->deviceInfoDisplayText(id));
            } else {
                label->setText({});
            }

            label->setVisible(!label->text().isEmpty());
        };

        connect(this, &Private::deviceInfoChanged, this, updateDeviceInfo);
        connect(devicesView, &DeviceConnectionView::currentDeviceChanged,
                this, [id, updateDeviceInfo] {
            updateDeviceInfo({id});
        });

        return label;
    };

    statusBar->addPermanentWidget(createStatusLabel(core::DeviceInfo::Temperature));
    statusBar->addPermanentWidget(createStatusLabel(core::DeviceInfo::MainTrackVoltage));
    statusBar->addPermanentWidget(createStatusLabel(core::DeviceInfo::MainTrackCurrentFiltered));
    statusBar->addPermanentWidget(createStatusLabel(core::DeviceInfo::TrackStatus, powerActionGroup->actions()));

    q()->setStatusBar(statusBar);
    // FIXME        variableEditorView->setStatusBar(statusBar);
}

void MainWindow::Private::setupFileReceiver()
{
    connect(fileSharing, &FileSharing::fileTransferOffered, this, &Private::onFileTransferRequested);

    trackPlanView->setFileSharing(fileSharing);
}

void MainWindow::Private::onFileTransferRequested(FileTransferSession *session)
{
    const auto info = session->fileInfo();

    auto question =
            tr("File transfer offered by %1: <b>%2</b> (%3). Do you want to receive this file?").
            arg(info.owningDevice().deviceName(), info.fileName(), prettyFileSize(info.fileSize()));

    if (QMessageBox::question(q(), tr("File Transfer Request"), std::move(question)) == QMessageBox::Yes) {
        connect(session, &FileTransferSession::downloadProgress,
                this, [this, session](qint64 bytesReceived, qint64 totalBytes) {
            onFileSharingProgress(session, bytesReceived, totalBytes);
        });

        connect(session, &FileTransferSession::finished, this, [this, session] {
            onFileSharingFinished(session);
        });

        session->accept();
    } else {
        session->reject();
    }
}

void MainWindow::Private::onFileSharingProgress(FileTransferSession *session, qint64 bytesReceived, qint64 totalBytes)
{
    qInfo() << __func__ << session << bytesReceived << totalBytes;
    const auto fileInfo = session->fileInfo();

    statusBar->showTemporaryMessage(tr("Receiving %1 from %2 (%3%)...").
                                    arg(fileInfo.fileName(), fileInfo.owningDevice().deviceName(),
                                        QString::number(100 * bytesReceived / totalBytes)));
}

void MainWindow::Private::onFileSharingFinished(FileTransferSession *session)
{
    const auto cleanup = qScopeGuard([session] { session->deleteLater(); });
    const auto info = session->fileInfo();

    auto question =
            tr("File transfer from %1 has finished. Do you want to open the received file <b>%2</b> now?").
            arg(info.owningDevice().deviceName(), info.fileName());

    if (QMessageBox::question(q(), tr("File Transfer Finished"), std::move(question)) == QMessageBox::Yes) {
        stack->setCurrentWidget(trackPlanView);
        trackPlanView->open(session->localFileName());
    }
}

void MainWindow::Private::onFileSharingEnabled()
{
    if (!fileSharingEnabledAction->isChecked()) {
        fileSharing->stopListening();
        statusBar->showTemporaryMessage(tr("File sharing with Roco Z21 App stopped."));
    } else if (fileSharing->startListening()) {
        statusBar->showTemporaryMessage(tr("Ready for file sharing with Roco Z21 App..."));
    } else {
        statusBar->showTemporaryMessage(tr("Could not start file sharing with Roco Z21 App."));
    }

    QSettings{}.setValue(s_settings_fileSharingEnabled, fileSharing->isListening());
}

void MainWindow::Private::restoreSettings()
{
    devicesView->restoreSettings();

    const auto settings = QSettings{};

    fileSharingEnabledAction->setChecked(settings.value(s_settings_fileSharingEnabled, true).toBool());
}

void MainWindow::Private::mergeActions(MainWindowView *view)
{
    for (const auto &[type, settings]: actionCategories.asKeyValueRange()) {
        if (const auto actionGroup = view->actionGroup(type)) {
            const auto menu = core::checked_cast<QMenu *>(settings.placeholder->parent());
            menu->insertActions(settings.placeholder, actionGroup->actions());
            actionCategories[type].actionGroups[view] += actionGroup;
        }
    }
}

void MainWindow::Private::onCurrentViewChanged()
{
    for (const auto &category: std::as_const(actionCategories)) {
        auto placeholderVisible = false;

        for (const auto &[view, actionGroups]: category.actionGroups.asKeyValueRange()) {
            const auto isCurrentView = (view == stack->currentWidget());

            for (const auto &group: actionGroups) {
                if (group) {
                    group->setVisible(isCurrentView);
                    placeholderVisible |= group->isVisible();
                }
            }
        }

        category.placeholder->setVisible(placeholderVisible);

        onFileNameChanged();
        onModifiedChanged();
    }
}

void MainWindow::Private::onFileNameChanged()
{
    if (const auto mainWindowView = dynamic_cast<MainWindowView *>(stack->currentWidget()))
        q()->setWindowFilePath(mainWindowView->fileName());
    else
        q()->setWindowFilePath({});

    auto windowTitle = qApp->applicationName();

    if (const auto fileName = q()->windowFilePath(); !fileName.isEmpty())
        windowTitle = QFileInfo{fileName}.fileName() + "[*] - "_L1 + windowTitle;

    q()->setWindowTitle(std::move(windowTitle));
}

void MainWindow::Private::onModifiedChanged()
{
    if (const auto mainWindowView = dynamic_cast<MainWindowView *>(stack->currentWidget()))
        q()->setWindowModified(mainWindowView->isModified());
    else
        q()->setWindowModified(false);
}

void MainWindow::Private::onCurrentDeviceChanged(core::Device *newDevice)
{
    if (const auto oldDevice = std::exchange(currentDevice, newDevice); oldDevice != newDevice) {
        if (oldDevice) {
            if (const auto control = oldDevice->powerControl())
                control->disconnect(this);
            if (const auto control = oldDevice->vehicleControl())
                control->disconnect(this);
            if (const auto control = oldDevice->variableControl())
                control->disconnect(this);

            oldDevice->disconnect(variableEditorView);
            oldDevice->disconnect(vehicleControlView);
            oldDevice->disconnect(this);
        }

        if (newDevice) {
            if (const auto control = newDevice->powerControl())
                connect(control, &core::PowerControl::stateChanged, this, &Private::onPowerStateChanged);

            connect(newDevice, &core::Device::deviceInfoChanged, this, &Private::deviceInfoChanged);
            connect(newDevice, &core::Device::stateChanged, this, &Private::onDeviceStateChanged);

// FIXME
//            connect(newDevice, &core::Device::variableControlChanged,
//                    variableEditorView, &VariableEditorView::setVariableControl);
//            connect(newDevice, &core::Device::vehicleControlChanged,
//                    vehicleControlView, &VehicleControlView::setVehicleControl);

            onDeviceStateChanged(deviceState());
            onPowerStateChanged();
        }
    }
}

void MainWindow::Private::onDeviceStateChanged(core::Device::State state)
{
    if (currentDevice) {
        qCInfo(logger()) << "state changed to" << state << "for" << currentDevice->name();

        switch (state) {
        case core::Device::State::Connecting:
            statusBar->showPermanentMessage(tr("Connecting to %1...").arg(currentDevice->name()));
            break;

        case core::Device::State::Connected:
            statusBar->showPermanentMessage(tr("Connected to %1").arg(currentDevice->name()));
            break;

        case core::Device::State::Disconnected:
            statusBar->clearPermanentMessage();
            statusBar->showTemporaryMessage(tr("Disconnected from %1").arg(currentDevice->name()));
            break;
        }
    } else {
        qCInfo(logger()) << "state changed to" << state;
    }

    deviceMenu->setEnabled(state == core::Device::State::Connected);
    powerActionGroup->setEnabled(powerControl() != nullptr);
// FIXME
//    variableEditorView->setVariableControl(variableControl());
//    vehicleControlView->setVehicleControl(vehicleControl());
//    speedMeterView->setSpeedMeter(speedMeter());

    if (const auto widget = stack->currentWidget(); !(widget && widget->isEnabled()))
        stack->setCurrentWidget(devicesView);
}

void MainWindow::Private::onCurrentVehicleChanged(core::dcc::VehicleAddress address)
{
    QSettings{}.setValue(s_settings_lastVehicleAddress, address.value);

// FIXME
//    variableEditorView->setCurrentVehicle(address);
//    vehicleControlView->setCurrentVehicle(address);
}

void MainWindow::Private::onPowerStateChanged()
{
    const auto control = powerControl();
    const auto state = (control ? control->state() : core::PowerControl::State::PowerOff);

    disablePowerAction->setChecked(state == core::PowerControl::State::PowerOff);
    enablePowerAction->setChecked(state == core::PowerControl::State::PowerOn);
    emergencyStopAction->setChecked(state == core::PowerControl::State::EmergencyStop);
    shortcircutAction->setChecked(state == core::PowerControl::State::ShortCircuit);
    serviceModeAction->setChecked(state == core::PowerControl::State::ServiceMode);

    shortcircutAction->setVisible(shortcircutAction->isChecked());
    serviceModeAction->setVisible(serviceModeAction->isChecked());
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}
    , d{new Private{this}}
{
    setCentralWidget(d->stack);
    addToolBar(Qt::LeftToolBarArea, d->navigation);

    d->setupActions();
    d->setupMenuBar();
    d->setupViews();
    d->setupStatusBar();
    d->setupFileReceiver();

    d->restoreSettings();

    // the following setup routines require settings being restored
    d->onCurrentViewChanged();
    d->onFileSharingEnabled();
    d->onDeviceStateChanged(d->deviceState());
}

MainWindow::~MainWindow()
{
    delete d;
}

QActionGroup *MainWindowView::actionGroup(ActionCategory) const
{
    return nullptr;
}

QString MainWindowView::fileName() const
{
    return {};
}

bool MainWindowView::isModified() const
{
    return false;
}

} // namespace lmrs::studio

#include "mainwindow.moc"
