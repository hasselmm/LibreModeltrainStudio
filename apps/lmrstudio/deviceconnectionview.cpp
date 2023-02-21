#include "deviceconnectionview.h"

#include "deviceparameterwidget.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/gui/fontawesome.h>

#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QFontMetrics>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMetaEnum>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>

namespace lmrs::studio {

namespace {

constexpr auto s_settingsDeviceType = "Connect/DeviceType"_L1;
constexpr auto s_settingsRecentSettings = "Connect/RecentSettings"_L1;

auto featureString(core::Device *device)
{
    QString features;

    if (device) {
        if (device->powerControl()) {
            features += ' '_L1;
            features += gui::fontawesome::fasPlug;
        }

        if (device->vehicleControl()) {
            features += ' '_L1;
            features += gui::fontawesome::fasTrain;
        }

        if (device->accessoryControl()) {
            features += ' '_L1;
            features += gui::fontawesome::fasTrafficLight;
        }

        if (device->detectorControl()) {
            features += ' '_L1;
            features += gui::fontawesome::fasBinoculars;
        }

        if (device->variableControl()) {
            features += ' '_L1;
            features += gui::fontawesome::fasScrewdriverWrench;
        }

        if (device->debugControl()) {
            features += ' '_L1;
            features += gui::fontawesome::fasBug;
        }

        if (device->speedMeterControl()) {
            features += ' '_L1;
            features += gui::fontawesome::fasGaugeHigh;
        }
    }

    return features;
}

class ConnectedDeviceModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum class Role {
        Name = Qt::DisplayRole,
        Icon = Qt::DecorationRole,
        Device = Qt::UserRole,
        Features,
    };

    using QAbstractListModel::QAbstractListModel;

    static core::Device *device(QModelIndex index);
    QModelIndex addDevice(core::Device *device);
    QModelIndex findDevice(QString uniqueId) const;

public: // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QList<QPointer<core::Device>> m_devices;
};

core::Device *ConnectedDeviceModel::device(QModelIndex index)
{
    return index.data(core::value(Role::Device)).value<core::Device *>();
}

QModelIndex ConnectedDeviceModel::addDevice(core::Device *device)
{
    if (!device)
        return {};
    if (m_devices.contains(device))
        return {};

    connect(device, &core::Device::destroyed, this, [this] {
        for (auto row = 0; (row = static_cast<int>(m_devices.indexOf(nullptr, row))) >= 0; ) {
            beginRemoveRows({}, row, row);
            m_devices.remove(row);
            endRemoveRows();
        }
    });

    connect(device, &core::Device::stateChanged, this, [this, device] {
        if (const auto row = static_cast<int>(m_devices.indexOf(device)); row >= 0)
            emit dataChanged(index(row), index(row), {core::value(Role::Features)});
    });

    const auto row = static_cast<int>(m_devices.count());
    beginInsertRows({}, row, row);
    m_devices.append(device);
    endInsertRows();

    return index(row);
}

QModelIndex ConnectedDeviceModel::findDevice(QString uniqueId) const
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if ((*it)->uniqueId() == uniqueId) {
            const auto row = static_cast<int>(it - m_devices.begin());
            return index(row);
        }
    }

    return {};
}

int ConnectedDeviceModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_devices.count()) + 1;
}

QVariant ConnectedDeviceModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        const auto device = index.row() < m_devices.count() ? m_devices[index.row()] : nullptr;

        switch (static_cast<Role>(role)) {
        case Role::Name:
            return device ? device->name() : tr("Add new device...");

        case Role::Icon:
            return gui::fontawesome::icon(device ? gui::fontawesome::fasMicrochip : gui::fontawesome::fasCirclePlus);

        case Role::Features:
            return featureString(device);

        case Role::Device:
            return QVariant::fromValue(device.get());
        }
    }

    return {};
}

class FilteredDeviceModel : public QSortFilterProxyModel
{
public:
    using DeviceFilter = DeviceConnectionView::DeviceFilter;

    explicit FilteredDeviceModel(DeviceFilter filter, QObject *parent = nullptr)
        : QSortFilterProxyModel{parent}
        , m_filter{filter}
    {}

    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    const DeviceFilter m_filter;
};

bool FilteredDeviceModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    const auto index = sourceModel()->index(row, 0, parent);
    const auto device = index.data(Qt::UserRole).value<core::Device *>();

    if (!device)
        return false;
    if (m_filter && !m_filter(device))
        return false;

    return true;
}

class ConnectedDeviceDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

public: // QAbstractItemDelegate interface
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QFont m_symbolFont = QFont{gui::fontawesome::solidFontFamily()};
};

void ConnectedDeviceDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    const auto restorePainter = qScopeGuard(std::bind(&QPainter::restore, painter));
    const auto style = option.widget ? option.widget->style() : QApplication::style();

    QStyleOptionViewItem localOption = option;
    initStyleOption(&localOption, index);

    auto text = std::exchange(localOption.text, {});
    const auto textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, option.widget) + 1;
    const auto textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &localOption, option.widget).
            adjusted(textMargin, 0, -textMargin, 0);

    auto features = featureString(ConnectedDeviceModel::device(index));
    auto featureRect = QFontMetrics{m_symbolFont}.boundingRect(features);

    style->drawControl(QStyle::CE_ItemViewItem, &localOption, painter, option.widget);

    featureRect.moveLeft(textRect.right() - featureRect.width());
    featureRect.setTop(textRect.top());
    featureRect.setHeight(textRect.height());

    painter->setFont(option.font);
    painter->drawText(textRect.adjusted(0, 0, -featureRect.width(), 0),
                      std::move(text), Qt::AlignLeft | Qt::AlignVCenter);

    auto color = painter->pen().color();
    color.setAlphaF(0.3f);
    painter->setPen(std::move(color));

    painter->setFont(m_symbolFont);
    painter->drawText(std::move(featureRect), std::move(features), Qt::AlignRight | Qt::AlignVCenter);
}

QSize ConnectedDeviceDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (const auto value = index.data(Qt::SizeHintRole); value.isValid())
        return value.toSize();

    auto localOption = option;
    initStyleOption(&localOption, index);
    const auto style = option.widget ? option.widget->style() : QApplication::style();
    const auto sizeHint = style->sizeFromContents(QStyle::CT_ItemViewItem, &localOption, {}, option.widget);

    const auto features = featureString(ConnectedDeviceModel::device(index));
    const auto featureRect = QFontMetrics{m_symbolFont}.boundingRect(features);
    return sizeHint + QSize{featureRect.width(), 0};
}

} // namespace

class DeviceConnectionView::Private : public core::PrivateObject<DeviceConnectionView>
{
public:
    using PrivateObject::PrivateObject;

    ConnectedDeviceModel *connectedDeviceModel() const { return core::checked_cast<ConnectedDeviceModel *>(connectedDevicesView->model()); }
    core::DeviceInfoModel *deviceInfoModel() const { return core::checked_cast<core::DeviceInfoModel *>(deviceInfoView->model()); }
    core::PowerControl *powerControl() const { return currentDevice ? currentDevice->powerControl() : nullptr; }

    void updateDeviceFactories();
    void updateWidgetsEnabled();
    void resizeDeviceInfoColumns();

    void onCurrentDeviceIndexChanged(const QModelIndex &index);
    void onDeviceSelectionChanged(const QItemSelection &selected);
    void onDeviceTypeChanged();
    void onRefreshButtonClicked();
    void onPowerSliderValueChanged(int value);
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();

    void onDeviceStateChanged(core::Device::State state);
    void onPowerStateChanged(core::PowerControl::State state);

    void storeSettings();
    void restoreSettings();

    core::ConstPointer<QListView> connectedDevicesView{q()};
    core::ConstPointer<QComboBox> deviceTypeBox{q()};
    core::ConstPointer<QStackedWidget> parameterStack{q()};
    core::ConstPointer<QTableView> deviceInfoView{q()};

    core::ConstPointer<QPushButton> refreshButton{tr("&Refresh"), q()};
    core::ConstPointer<QSlider> powerSlider{Qt::Orientation::Horizontal, q()};
    core::ConstPointer<QPushButton> connectButton{tr("&Connect"), q()};
    core::ConstPointer<QPushButton> disconnectButton{tr("&Disconnect"), q()};

    QHash<DeviceFilter, QAbstractItemModel *> filteredDeviceModels;
    QPointer<core::Device> currentDevice;
};

void DeviceConnectionView::Private::updateDeviceFactories()
{
    for (const auto factory: core::DeviceFactory::deviceFactories()) {
        const auto widget = new DeviceParameterWidget{factory, parameterStack};
        deviceTypeBox->addItem(factory->name(), QVariant::fromValue(widget));
        connect(widget, &DeviceParameterWidget::activated, connectButton, &QPushButton::click);
        connect(widget, &DeviceParameterWidget::hasAcceptableInputChanged, this, &Private::updateWidgetsEnabled);
        parameterStack->addWidget(widget);
    }

    updateWidgetsEnabled();
}

void DeviceConnectionView::Private::onDeviceTypeChanged()
{
    parameterStack->setCurrentWidget(deviceTypeBox->currentData().value<QWidget *>());
    updateWidgetsEnabled();
}

void DeviceConnectionView::Private::updateWidgetsEnabled()
{
    const auto isDisconnected = currentDevice.isNull()
            || currentDevice->state() == core::Device::State::Disconnected;
    const auto isConnected = !currentDevice.isNull()
            && currentDevice->state() == core::Device::State::Connected;

    deviceTypeBox->setEnabled(isDisconnected);
    parameterStack->setEnabled(isDisconnected);
    refreshButton->setEnabled(!isDisconnected);
    powerSlider->setEnabled(isConnected && powerControl());
    connectButton->setEnabled(isDisconnected && q()->hasAcceptableInput());
    disconnectButton->setEnabled(!isDisconnected);
    deviceInfoView->setEnabled(isConnected);
}

void DeviceConnectionView::Private::resizeDeviceInfoColumns()
{
    QTimer::singleShot(0, deviceInfoView, [this] {
        deviceInfoView->resizeColumnToContents(value(core::DeviceInfoModel::Column::Name));
    });
}

void DeviceConnectionView::Private::onCurrentDeviceIndexChanged(const QModelIndex &index)
{
    const auto guard = propertyGuard(q(), &DeviceConnectionView::currentDevice,
                                     &DeviceConnectionView::currentDeviceChanged);

    currentDevice = ConnectedDeviceModel::device(index);
    deviceInfoModel()->setDevice(currentDevice);

    const auto factories = core::DeviceFactory::deviceFactories();

    if (const auto i = factories.indexOf(currentDevice ? currentDevice->factory() : nullptr); i >= 0)
        deviceTypeBox->setCurrentIndex(static_cast<int>(i));

    updateWidgetsEnabled();
}

void DeviceConnectionView::Private::onDeviceSelectionChanged(const QItemSelection &selected)
{
    if (selected.isEmpty()) {
        auto currentIndex = connectedDevicesView->currentIndex();

        if (!currentIndex.isValid())
            currentIndex = connectedDeviceModel()->index(connectedDeviceModel()->rowCount() - 1);

        connectedDevicesView->selectionModel()->select(std::move(currentIndex), QItemSelectionModel::SelectCurrent);
    }
}

void DeviceConnectionView::Private::onRefreshButtonClicked()
{
    currentDevice->updateDeviceInfo();
}

void DeviceConnectionView::Private::onPowerSliderValueChanged(int value)
{
    if (const auto powerControl = Private::powerControl()) {
        if (value > 0)
            powerControl->enableTrackPower(core::retryOnError);
        else
            powerControl->disableTrackPower(core::retryOnError);
    }
}

void DeviceConnectionView::Private::onConnectButtonClicked()
{
    storeSettings();

    if (const auto widget = dynamic_cast<DeviceParameterWidget *>(parameterStack->currentWidget())) {
        const auto expectedUniqueId = widget->factory()->uniqueId(widget->parameters());
        if (auto index = connectedDeviceModel()->findDevice(expectedUniqueId); index.isValid()) {
            connectedDevicesView->setCurrentIndex(std::move(index));
            return;
        }

        if (const auto device = widget->factory()->create(widget->parameters(), this)) {
            Q_ASSERT(device->uniqueId() == expectedUniqueId);

            if (const auto powerControl = device->powerControl())
                connect(powerControl, &core::PowerControl::stateChanged, this, &Private::onPowerStateChanged);

            connect(device, &core::Device::stateChanged, this, &Private::onDeviceStateChanged);

            auto index = connectedDeviceModel()->addDevice(device);
            connectedDevicesView->setCurrentIndex(std::move(index));
            device->connectToDevice();
        }
    }
}

void DeviceConnectionView::Private::onDisconnectButtonClicked()
{
    const auto guard = propertyGuard(q(), &DeviceConnectionView::currentDevice,
                                     &DeviceConnectionView::currentDeviceChanged);

    if (currentDevice) {
        if (const auto powerControl = currentDevice->powerControl())
            powerControl->disconnect(this);

        currentDevice->disconnectFromDevice();
        currentDevice->disconnect(this);
        currentDevice->deleteLater();
        currentDevice.clear();
    }

    updateWidgetsEnabled();
}

void DeviceConnectionView::Private::onDeviceStateChanged(core::Device::State)
{
    updateWidgetsEnabled();
}

void DeviceConnectionView::Private::onPowerStateChanged(core::PowerControl::State state)
{
    powerSlider->setValue(state == core::PowerControl::State::PowerOn ? 1 : 0);
}

void DeviceConnectionView::Private::storeSettings()
{
    if (const auto widget = dynamic_cast<DeviceParameterWidget *>(parameterStack->currentWidget())) {
        const auto deviceType = QString::fromLatin1(widget->factory()->metaObject()->className());

        auto settings = QSettings{};

        settings.setValue(s_settingsDeviceType, deviceType);
        settings.beginGroup(s_settingsRecentSettings);
        settings.setValue(deviceType, widget->parameters());
        settings.endGroup();
    }
}

void DeviceConnectionView::Private::restoreSettings()
{
    auto settings = QSettings{};

    const auto deviceType = settings.value(s_settingsDeviceType).toString();

    for (auto factoryWidget: q()->widgets()) {
        const auto factoryType = QString::fromLatin1(factoryWidget->factory()->metaObject()->className());

        settings.beginGroup(s_settingsRecentSettings);
        factoryWidget->setParameters(settings.value(factoryType).toMap());
        settings.endGroup();

        if (factoryType == deviceType)
            q()->setCurrentWidget(factoryWidget);
    }
}

DeviceConnectionView::DeviceConnectionView(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    d->connectedDevicesView->setItemDelegate(new ConnectedDeviceDelegate{d->connectedDevicesView});
    d->connectedDevicesView->setSelectionMode(QTableView::SelectionMode::SingleSelection);
    d->connectedDevicesView->setSelectionBehavior(QTableView::SelectionBehavior::SelectRows);
    d->connectedDevicesView->setModel(new ConnectedDeviceModel{d->connectedDevicesView});

    d->deviceInfoView->setShowGrid(false);
    d->deviceInfoView->setAlternatingRowColors(true);
    d->deviceInfoView->verticalHeader()->hide();
    d->deviceInfoView->setSelectionMode(QTableView::SelectionMode::NoSelection);
    d->deviceInfoView->setSelectionBehavior(QTableView::SelectionBehavior::SelectRows);
    d->deviceInfoView->setModel(new core::DeviceInfoModel{d->deviceInfoView});
    d->deviceInfoView->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    d->deviceInfoView->setMinimumSize(360, 400);

    d->deviceInfoView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    d->deviceInfoView->horizontalHeader()->setStretchLastSection(true);
    d->deviceInfoView->horizontalHeader()->hide();

    d->powerSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    d->powerSlider->setMaximumWidth(30);
    d->powerSlider->setRange(0, 1);

    const auto powerLabel = new QLabel{tr("&Power:"), this};
    powerLabel->setBuddy(d->powerSlider);

    d->updateDeviceFactories();

    const auto buttonLayout = new QHBoxLayout;

    buttonLayout->addWidget(d->refreshButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(powerLabel);
    buttonLayout->addWidget(d->powerSlider);
    buttonLayout->addWidget(d->connectButton);
    buttonLayout->addWidget(d->disconnectButton);

    const auto layout = new QVBoxLayout{this};

    layout->addWidget(d->connectedDevicesView, 1);
    layout->addSpacing(8);
    layout->addWidget(d->deviceTypeBox);
    layout->addWidget(d->parameterStack);
    layout->addSpacing(8);
    layout->addLayout(buttonLayout);
    layout->addWidget(d->deviceInfoView, 5);

    const auto deviceSelectionModel = d->connectedDevicesView->selectionModel();
    connect(deviceSelectionModel, &QItemSelectionModel::currentChanged, d, &Private::onCurrentDeviceIndexChanged);
    connect(deviceSelectionModel, &QItemSelectionModel::selectionChanged, d, &Private::onDeviceSelectionChanged);

    connect(d->deviceTypeBox, &QComboBox::currentIndexChanged, d, &Private::onDeviceTypeChanged);
    connect(d->deviceInfoModel(), &core::DeviceInfoModel::rowsInserted, d, &Private::resizeDeviceInfoColumns);
    connect(d->parameterStack, &QStackedWidget::currentChanged, this, &DeviceConnectionView::currentWidgetChanged);
    connect(d->parameterStack, &QStackedWidget::currentChanged, d->deviceTypeBox, &QComboBox::setCurrentIndex);

    connect(d->refreshButton, &QPushButton::clicked, d, &Private::onRefreshButtonClicked);
    connect(d->powerSlider, &QSlider::valueChanged, d, &Private::onPowerSliderValueChanged);
    connect(d->connectButton, &QPushButton::clicked, d, &Private::onConnectButtonClicked);
    connect(d->disconnectButton, &QPushButton::clicked, d, &Private::onDisconnectButtonClicked);

    d->onDeviceSelectionChanged({});
    d->onDeviceTypeChanged();
    d->updateWidgetsEnabled();
}

DeviceConnectionView::~DeviceConnectionView()
{
    delete d;
}

QAbstractItemModel *lmrs::studio::DeviceConnectionView::model(bool (*filter)(const core::Device *)) const
{
    if (const auto model = d->filteredDeviceModels.value(filter))
        return model;

    const auto model = new FilteredDeviceModel{filter, d};
    model->setSourceModel(d->connectedDeviceModel());
    d->filteredDeviceModels.insert(filter, model);
    return model;
}

QAbstractItemModel *DeviceConnectionView::model() const
{
    return model(nullptr);
}

core::Device *DeviceConnectionView::currentDevice() const
{
    return d->currentDevice;
}

bool DeviceConnectionView::hasAcceptableInput() const
{
    if (const auto widget = currentWidget(); widget && !widget->hasAcceptableInput())
        return false;

    return true;
}

QList<DeviceParameterWidget *> DeviceConnectionView::widgets() const
{
    QList<DeviceParameterWidget *> widgetList;
    widgetList.reserve(d->parameterStack->count());

    for (int i = 0; i < d->parameterStack->count(); ++i) {
        if (const auto widget = dynamic_cast<DeviceParameterWidget *>(d->parameterStack->widget(i)))
            widgetList.append(widget);
    }

    return widgetList;
}

void DeviceConnectionView::setCurrentWidget(DeviceParameterWidget *widget)
{
    d->parameterStack->setCurrentWidget(widget);
}

DeviceParameterWidget *DeviceConnectionView::currentWidget() const
{
    return dynamic_cast<DeviceParameterWidget *>(d->parameterStack->currentWidget());
}

void DeviceConnectionView::storeSettings()
{
    d->storeSettings();
}

void DeviceConnectionView::restoreSettings()
{
    d->restoreSettings();
}

void DeviceConnectionView::connectToDevice()
{
    d->onConnectButtonClicked();
}

void DeviceConnectionView::disconnectFromDevice()
{
    d->onDisconnectButtonClicked();
}

} // namespace lmrs::studio

#include "deviceconnectionview.moc"
#include "moc_deviceconnectionview.cpp"
