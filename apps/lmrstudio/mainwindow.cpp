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
#include <lmrs/gui/localization.h>

#include <lmrs/roco/z21appfilesharing.h>

#include <lmrs/widgets/actionutils.h>
#include <lmrs/widgets/documentmanager.h>
#include <lmrs/widgets/navigationtoolbar.h>
#include <lmrs/widgets/statusbar.h>

#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
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

auto prettyUrl(QString url)
{
    return url.replace("https://"_L1, QString{});
}

auto htmlLink(QString url) // FIXME: maybe move to HTML utility header
{
    auto text = prettyUrl(LMRS_HOMEPAGE_URL);
    return R"(<a href="%1">%2</a>)"_L1.arg(std::move(url), std::move(text));
}

auto visibleActionCount(const QList<QAction *>::const_iterator first, const QList<QAction *>::const_iterator last)
{
    return std::count_if(first, last, std::mem_fn(&QAction::isVisible));
}

auto countVisibleActions(const QList<QAction *> &actions)
{
    return visibleActionCount(actions.begin(), actions.end());
}

template<typename T>
requires std::is_pointer_v<T>
T get_if(const QList<T> &list, int i)
{
    if (i < list.size())
        return list.at(i);

    return nullptr;
}

} // namespace

class MainWindow::Private : public core::PrivateObject<MainWindow>
{
    Q_OBJECT

    static_assert(core::IsPrivateObject<Private>);
    static_assert(&staticMetaObject == &MainWindow::Private::staticMetaObject);
    static_assert(&staticMetaObject != &QObject::staticMetaObject);
    static_assert(LMRS_STATIC_METAOBJECT == &MainWindow::staticMetaObject);

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
    void setupToolBar();
    void setupStatusBar();
    void setupFileReceiver();

    void restoreSettings();

    void updateToolBarVisibility();

signals:
    void deviceInfoChanged(QList<lmrs::core::DeviceInfo> changedIds);

public:
    void mergeActions(MainWindowView *view);

    void onCurrentViewChanged();
    void onFileNameChanged();
    void onModifiedChanged();

    void onCurrentDeviceIndexChanged(int newIndex);
    void onCurrentDeviceChanged(core::Device *newDevice);
    void onDeviceStateChanged(core::Device::State state);
    void onCurrentVehicleChanged(core::dcc::VehicleAddress address);
    void onPowerStateChanged();

    void onFileTransferRequested(FileTransferSession *session);
    void onFileSharingProgress(FileTransferSession *session, qint64 bytesReceived, qint64 totalBytes);
    void onFileSharingFinished(FileTransferSession *session);
    void onSettingsFileSharingChecked(bool checked);

    void onDetachView();

    void onAboutApplication();
    void onAboutQt();

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
    core::ConstPointer<AutomationView> automationView{q()};
    core::ConstPointer<QToolBar> toolBar{q()};
    core::ConstPointer<StatusBar> statusBar{q()};

    core::ConstPointer<QActionGroup> powerActionGroup{this};
    core::ConstPointer<l10n::Action> enablePowerAction{icon(gui::fontawesome::fasPlugCircleBolt), LMRS_TR("&Enable track power"), powerActionGroup};
    core::ConstPointer<l10n::Action> disablePowerAction{icon(gui::fontawesome::fasPlugCircleMinus), LMRS_TR("&Disable track power"), powerActionGroup};
    core::ConstPointer<l10n::Action> emergencyStopAction{icon(gui::fontawesome::fasPlugCircleXmark), LMRS_TR("Emergency &stop"), powerActionGroup};
    core::ConstPointer<l10n::Action> shortCircuitAction{icon(gui::fontawesome::fasPlugCircleExclamation), LMRS_TR("Short Circuit"), powerActionGroup};
    core::ConstPointer<l10n::Action> serviceModeAction{icon(gui::fontawesome::fasScrewdriverWrench), LMRS_TR("Service mode"), powerActionGroup};

    core::ConstPointer<l10n::Action> settingsLanguageMenuAction{LMRS_TR("&Language"), q()};
    core::ConstPointer<l10n::Action> settingsFileSharingAction{LMRS_TR("&Z21 File Sharing"), this, &Private::onSettingsFileSharingChecked};

    QPointer<l10n::LanguageManager> languageManager;
    core::ConstPointer<FileSharing> fileSharing{this};
    core::ConstPointer<QActionGroup> settingsLanguageGroup{this};

    core::ConstPointer<SpacerAction> deviceSpacerAction{toolBar};
    core::ConstPointer<ComboBoxAction> deviceBoxAction{toolBar};
    core::ConstPointer<l10n::Action> detachViewAction{icon(gui::fontawesome::fasArrowUpRightFromSquare),
                LMRS_TR("&Detach view"), LMRS_TR("Detach current view from main window"),
                this, &Private::onDetachView};

    struct ActionCategory
    {
        using Type = MainWindowView::ActionCategory;

        QPointer<QAction> menuPlaceholder;
        QPointer<QAction> toolBarPlaceholder;
        QHash<QWidget *, QList<QPointer<QActionGroup>>> actionGroups;
    };

    QHash<ActionCategory::Type, ActionCategory> actionCategories;

    QMenu *addMenu(const l10n::String &text)
    {
        auto menu = std::make_unique<l10n::Facade<QMenu>>(text, q()->menuBar());
        q()->menuBar()->addMenu(menu.get());
        return menu.release();
    }

    template<class Parent, class... Args>
    QAction *addAction(Parent *parent, Args... args)
    {
        auto action = std::make_unique<l10n::Action>(args...);
        parent->addAction(action.get());
        action->setParent(parent);
        return action.release();
    }

    core::ConstPointer<QMenu> fileMenu{addMenu(LMRS_TR("&File"))};
    core::ConstPointer<QMenu> editMenu{addMenu(LMRS_TR("&Edit"))};
    core::ConstPointer<QMenu> viewMenu{addMenu(LMRS_TR("&View"))};
    core::ConstPointer<QMenu> deviceMenu{addMenu(LMRS_TR("&Device"))};

    bool blockCurrentDeviceIndexChanged : 1 = false;
};

void MainWindow::Private::setupActions()
{
    for (const auto action: powerActionGroup->actions())
        action->setCheckable(true);

    powerActionGroup->setExclusive(true);
    emergencyStopAction->setEnabled(false);
    shortCircuitAction->setEnabled(false);
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

    const auto notImplemented = new l10n::Facade<QLabel>{LMRS_TR("This screen has not been implemented yet"), q()};
    notImplemented->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    notImplemented->setEnabled(false);
    notImplemented->setMargin(20);

    const auto addView = [this](QIcon icon, l10n::String text, QWidget *view) {
        if (const auto mainWindowView = dynamic_cast<MainWindowView *>(view)) {
            connect(mainWindowView, &MainWindowView::fileNameChanged, this, &Private::onFileNameChanged);
            connect(mainWindowView, &MainWindowView::modifiedChanged, this, &Private::onModifiedChanged);
            mergeActions(mainWindowView);
        }

        navigation->addView(std::move(icon), std::move(text), view);
        stack->addWidget(view);
    };

    addView(icon(gui::fontawesome::fasPlug), LMRS_TR("&Connect"), devicesView);
    addView(icon(gui::fontawesome::fasTrain), LMRS_TR("&Drive"), vehicleControlView);
    addView(icon(gui::fontawesome::fasTrafficLight), LMRS_TR("S&ignal Box"), accessoryControlView);
    addView(icon(gui::fontawesome::fasGaugeHigh), LMRS_TR("Speed&meter"), speedMeterView);
    addView(icon(gui::fontawesome::fasScrewdriverWrench), LMRS_TR("&Programming"), variableEditorView);
    addView(icon(gui::fontawesome::fasBug), LMRS_TR("De&bug"), debugView);

    navigation->addSeparator();

    addView(icon(gui::fontawesome::fasCodeBranch), LMRS_TR("&Track Plan"), trackPlanView);
    addView(icon(gui::fontawesome::fasRobot), LMRS_TR("&Automation"), automationView);

    navigation->addSeparator();

    addView(icon(gui::fontawesome::fasCircleInfo), LMRS_TR("&Summary"), notImplemented);
    addView(icon(gui::fontawesome::fasTableList), LMRS_TR("&Functions"), functionMappingView);
    addView(icon(gui::fontawesome::fasVolumeHigh), LMRS_TR("&Sounds"), notImplemented);
    addView(icon(gui::fontawesome::fasFolderTree), LMRS_TR("&Explorer"), notImplemented);

    navigation->addSeparator();

    addView(icon(gui::fontawesome::fasBook), LMRS_TR("&Decoder\nLibrary"), decoderDatabaseView);

    navigation->setCurrentView(stack->currentWidget());
    navigation->attachMainWidget(stack);

    viewMenu->addActions(navigation->menuActions());

    connect(devicesView, &DeviceConnectionView::currentDeviceChanged, this, &Private::onCurrentDeviceChanged); // FIXME: this has to be done differently

// FIXME    connect(vehicleControlView, &VehicleControlView::currentVehicleChanged, this, &Private::onCurrentVehicleChanged);
// FIXME    connect(variableEditorView, &VariableEditorView::currentVehicleChanged, this, &Private::onCurrentVehicleChanged);

    onCurrentVehicleChanged(QSettings{}.value(s_settings_lastVehicleAddress, 3).
                            value<core::dcc::VehicleAddress::value_type>());
}

void MainWindow::Private::setupMenuBar()
{
    // -----------------------------------------------------------------------------------------------------------------

    actionCategories[ActionCategory::Type::FileNew].menuPlaceholder = fileMenu->addSeparator();
    actionCategories[ActionCategory::Type::FileOpen].menuPlaceholder = fileMenu->addSeparator();
    actionCategories[ActionCategory::Type::FileSave].menuPlaceholder = fileMenu->addSeparator();

    addAction(fileMenu.get(), LMRS_TR("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    // -----------------------------------------------------------------------------------------------------------------

    actionCategories[ActionCategory::Type::EditCreate].menuPlaceholder = editMenu->addSeparator();
    actionCategories[ActionCategory::Type::EditClipboard].menuPlaceholder = editMenu->addSeparator();

    // -----------------------------------------------------------------------------------------------------------------

    actionCategories[ActionCategory::Type::View].menuPlaceholder = viewMenu->addSeparator();

    deviceMenu->addActions(powerActionGroup->actions());
    deviceMenu->addSeparator();

    addAction(deviceMenu.get(), LMRS_TR("Dis&connect"), devicesView.get(), &DeviceConnectionView::disconnectFromDevice);

    const auto settingsMenu = addMenu(LMRS_TR("&Settings")); // --------------------------------------------------------

    settingsLanguageMenuAction->setMenu(new QMenu{q()});
    settingsLanguageMenuAction->setVisible(false);

    settingsMenu->addAction(settingsLanguageMenuAction);
    settingsMenu->addAction(settingsFileSharingAction);

    const auto helpMenu = addMenu(LMRS_TR("&Help")); // ----------------------------------------------------------------

    addAction(helpMenu, LMRS_TR("&About %1...").filtered([](QString &text) {
                  return text.arg(qApp->applicationDisplayName());
              }), this, &Private::onAboutApplication);

    addAction(helpMenu, LMRS_TR("About &Qt..."), this, &Private::onAboutQt);
}

void MainWindow::Private::setupToolBar()
{
    toolBar->setFloatable(false);
    toolBar->setMovable(false);

    actionCategories[ActionCategory::Type::FileNew].toolBarPlaceholder = toolBar->addSeparator();
    actionCategories[ActionCategory::Type::FileOpen].toolBarPlaceholder = toolBar->addSeparator();
    actionCategories[ActionCategory::Type::FileSave].toolBarPlaceholder = toolBar->addSeparator();
    actionCategories[ActionCategory::Type::EditCreate].toolBarPlaceholder = toolBar->addSeparator();
    actionCategories[ActionCategory::Type::EditClipboard].toolBarPlaceholder = toolBar->addSeparator();
    actionCategories[ActionCategory::Type::View].toolBarPlaceholder = toolBar->addSeparator();

    for (const auto &category: std::as_const(actionCategories))
        if (const auto placeholder = category.toolBarPlaceholder)
            placeholder->setVisible(false);

    deviceBoxAction->setSizePolicy(QSizePolicy::MinimumExpanding);
    deviceBoxAction->setMinimumContentsLength(25);

    toolBar->addAction(deviceSpacerAction);
    toolBar->addAction(deviceBoxAction);
    toolBar->addAction(detachViewAction);

    connect(deviceBoxAction, &QAction::visibleChanged, this, &Private::updateToolBarVisibility);
    connect(detachViewAction, &QAction::enabledChanged, this, &Private::updateToolBarVisibility);

    connect(deviceBoxAction, &ComboBoxAction::currentIndexChanged, this, &Private::onCurrentDeviceIndexChanged);

    updateToolBarVisibility();
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

void MainWindow::Private::onSettingsFileSharingChecked(bool checked)
{
    if (!checked) {
        fileSharing->stopListening();
        statusBar->showTemporaryMessage(tr("File sharing with Roco Z21 App stopped."));
    } else if (fileSharing->startListening()) {
        statusBar->showTemporaryMessage(tr("Ready for file sharing with Roco Z21 App..."));
    } else {
        statusBar->showTemporaryMessage(tr("Could not start file sharing with Roco Z21 App."));
    }

    QSettings{}.setValue(s_settings_fileSharingEnabled, fileSharing->isListening());
}

void MainWindow::Private::onDetachView()
{
    // FIXME: implement detaching
}

void MainWindow::Private::onAboutApplication()
{
    auto title = tr("About %1").arg(qApp->applicationDisplayName());
    auto invitation = tr("Visit %1 for information.").arg(htmlLink(LMRS_HOMEPAGE_URL));
    auto text = "<p><b>%1 %2</b></p><p>%3</p>"_L1.arg(qApp->applicationDisplayName(),
                                                      qApp->applicationVersion(),
                                                      std::move(invitation));

    QMessageBox::about(q(), std::move(title), std::move(text));
}

void MainWindow::Private::onAboutQt()
{
    QMessageBox::aboutQt(q());
}

void MainWindow::Private::restoreSettings()
{
    devicesView->restoreSettings();

    const auto settings = QSettings{};

    settingsFileSharingAction->setChecked(settings.value(s_settings_fileSharingEnabled, true).toBool());
}

void MainWindow::Private::updateToolBarVisibility()
{
    const auto alwaysVisible = std::array{deviceSpacerAction.get<QAction>(), detachViewAction.get<QAction>()};
    const auto alwaysVisibleActionCount = static_cast<int>(alwaysVisible.size());
    const auto visibleActionCount = countVisibleActions(toolBar->actions());

    toolBar->setVisible(visibleActionCount > alwaysVisibleActionCount || detachViewAction->isEnabled());
    toolBar->setMinimumWidth(toolBar->isVisible() ? toolBar->sizeHint().width() : 0);
}

void MainWindow::Private::mergeActions(MainWindowView *view)
{
    for (const auto &[type, settings]: actionCategories.asKeyValueRange()) {
        if (const auto actionGroup = view->actionGroup(type)) {
            if (const auto placeholder = settings.menuPlaceholder) {
                const auto menu = core::checked_cast<QMenu *>(placeholder->parent());

                for (const auto actionList = actionGroup->actions(); const auto action: actionList) {
                    if (!dynamic_cast<QWidgetAction *>(action))
                        menu->insertAction(placeholder, action);
                }
            }

            if (const auto placeholder = settings.toolBarPlaceholder) {
                const auto actions = actionGroup->actions();
                for (auto i = 0; i < actions.size(); ++i) {
                    auto currentAction = actions.at(i);

                    if (currentAction->icon().isNull()
                            && !dynamic_cast<QWidgetAction *>(currentAction))
                        continue; // skip actions like "recently used files", that purposefully have no icon

                    if (const auto nextAction = get_if(actions, i + 1)) {
                        if (nextAction->menu()
                            && nextAction->icon().isNull()) {
                            currentAction = createProxyAction(currentAction, toolBar);
                            currentAction->setMenu(nextAction->menu());
                        }
                    }

                    toolBar->insertAction(placeholder, currentAction);
                }
            }

            actionCategories[type].actionGroups[view] += actionGroup;
        }
    }
}

void MainWindow::Private::onCurrentViewChanged()
{
    // Update model and selection of the toolbar's device box.

    blockCurrentDeviceIndexChanged = true; // Avoid that the globally selected device changes.

    if (const auto mainWindowView = dynamic_cast<const MainWindowView *>(stack->currentWidget())) {
        deviceBoxAction->setModel(devicesView->model(mainWindowView->deviceFilter()));
        detachViewAction->setEnabled(mainWindowView->isDetachable());
    } else {
        deviceBoxAction->setModel(devicesView->model(DeviceFilter::none()));
        detachViewAction->setEnabled(false);
    }

    if (const auto model = dynamic_cast<const DeviceModelInterface *>(deviceBoxAction->model())) {
        auto index = model->indexOf(devicesView->currentDevice());
        deviceBoxAction->setCurrentIndex(std::move(index));
    }

    blockCurrentDeviceIndexChanged = false; // Done. Restore the signal connection.

    // Ensure, only the current view's actions are visible, hide all other actions.

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

        category.menuPlaceholder->setVisible(placeholderVisible);
    }

    // Disable the "Edit" menu, if there are no edit actions.

    editMenu->setEnabled(countVisibleActions(editMenu->actions()) > 0);

    // Show the neccessary toolbar separators.

    const auto toolBarActions = toolBar->actions();
    const auto findToolBarPlaceholder = [this, toolBarActions](auto categoryType) {
        const auto placeHolder = actionCategories.value(categoryType).toolBarPlaceholder;
        return toolBarActions.begin() + toolBarActions.indexOf(placeHolder);
    };

    const auto filePlaceholderIter = findToolBarPlaceholder(ActionCategory::Type::FileSave);
    const auto editPlaceholderIter = findToolBarPlaceholder(ActionCategory::Type::EditClipboard);
    const auto viewPlaceholderIter = findToolBarPlaceholder(ActionCategory::Type::View);

    const auto hasFileActions = (visibleActionCount(toolBarActions.begin(), filePlaceholderIter) > 0);
    const auto hasEditActions = (visibleActionCount(filePlaceholderIter + 1, editPlaceholderIter) > 0);
    const auto hasViewActions = (visibleActionCount(editPlaceholderIter + 1, viewPlaceholderIter) > 0);

    (*filePlaceholderIter)->setVisible(hasFileActions && hasEditActions);
    (*editPlaceholderIter)->setVisible(hasEditActions && hasViewActions);

    // do not show the toolbar if not needed
    updateToolBarVisibility();

    // Update the window title and such with current filename.
    onFileNameChanged();
    onModifiedChanged();
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

void MainWindow::Private::onCurrentDeviceIndexChanged(int newIndex)
{
    if (blockCurrentDeviceIndexChanged)
        return;

    if (const auto model = deviceBoxAction->model())
        if (const auto device = DeviceModelInterface::device(model->index(newIndex, 0)))
            devicesView->setCurrentDevice(device);
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

        if (const auto model = dynamic_cast<DeviceModelInterface *>(deviceBoxAction->model())) {
            qInfo() << Q_FUNC_INFO << "device found at" << model->indexOf(newDevice) << (newDevice ? newDevice->name() : "<null>"_L1);
            deviceBoxAction->setCurrentIndex(model->indexOf(newDevice));
        }
    }
}

void MainWindow::Private::onDeviceStateChanged(core::Device::State state)
{
    if (currentDevice) {
        qCInfo(logger(), "State changed to %s for \"%ls\"", core::key(state), qUtf16Printable(currentDevice->name()));

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
        qCInfo(logger(), "State changed to %s without device", core::key(state));
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
    shortCircuitAction->setChecked(state == core::PowerControl::State::ShortCircuit);
    serviceModeAction->setChecked(state == core::PowerControl::State::ServiceMode);

    shortCircuitAction->setVisible(shortCircuitAction->isChecked());
    serviceModeAction->setVisible(serviceModeAction->isChecked());
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}
    , d{new Private{this}}
{
    auto centralWidget = new QWidget{this};
    auto centralLayout = new QVBoxLayout{centralWidget};
    centralLayout->setContentsMargins({});
    centralLayout->addWidget(d->toolBar);
    centralLayout->addWidget(d->stack, 1);
    setCentralWidget(centralWidget);

    addToolBar(Qt::LeftToolBarArea, d->navigation);

    d->setupActions();
    d->setupMenuBar();
    d->setupToolBar();
    d->setupViews();
    d->setupStatusBar();
    d->setupFileReceiver();

    d->restoreSettings();

    // the following setup routines require settings being restored
    d->onCurrentViewChanged();
    d->onDeviceStateChanged(d->deviceState());
    d->onSettingsFileSharingChecked(d->settingsFileSharingAction->isChecked());
}

MainWindow::~MainWindow()
{
    delete d;
}

void MainWindow::setLanguageManager(l10n::LanguageManager *languageManager)
{
    if (std::exchange(d->languageManager, languageManager) == d->languageManager)
        return;

    const auto languageMenu = d->settingsLanguageMenuAction->menu();

    languageMenu->clear();

    for (auto action: d->settingsLanguageGroup->actions()) {
        d->settingsLanguageGroup->removeAction(action);
        delete action;
    }

    for (const auto &language: languageManager->availableLanguages()) {
        auto text = language.nativeName + " ("_L1 + language.englishName + ")"_L1;
        auto action = new QAction{std::move(text), d->settingsLanguageGroup};

        action->setCheckable(true);
        action->setChecked(language.id == languageManager->currentLanguage());

        connect(action, &QAction::triggered, this, [this, language = language.id](bool checked) {
            if (checked)
                d->languageManager->setCurrentLanguage(language);
        });

        languageMenu->addAction(action);
    }

    d->settingsLanguageMenuAction->setVisible(!languageMenu->isEmpty());
}

l10n::LanguageManager *MainWindow::languageManager() const
{
    return d->languageManager;
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

void MainWindowView::setCurrentDevice(core::Device *)
{
    // nothing to do by default
}

DeviceFilter MainWindowView::deviceFilter() const
{
    return DeviceFilter::none();
}

bool MainWindowView::isDetachable() const
{
    return false;
}

void MainWindowView::connectDocumentManager(widgets::DocumentManager *manager)
{
    connect(manager, &widgets::DocumentManager::fileNameChanged, this, [this](QString fileName) {
        emit fileNameChanged(std::move(fileName), {});
    });

    connect(manager, &widgets::DocumentManager::modifiedChanged, this, [this](bool modified) {
        emit modifiedChanged(modified, {});
    });
}

} // namespace lmrs::studio

#include "mainwindow.moc"
