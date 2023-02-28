#include "vehiclecontrolview.h"

#include "deviceconnectionview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/vehicleinfomodel.h>

#include <lmrs/gui/fontawesome.h>

#include <lmrs/widgets/speeddial.h>
#include <lmrs/widgets/spinbox.h>

#include <QBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QTableView>

namespace lmrs::studio {

namespace dcc = core::dcc;

using core::VehicleInfoModel;
using widgets::SpeedDial;
using widgets::SpinBox;

namespace {

constexpr auto s_settingsVehiclesDevices = "Vehicles/"_L1;
constexpr auto s_settingsSubkeyName = "/Name"_L1;

auto vehicleGroupKey(core::Device *device)
{
    return s_settingsVehiclesDevices + device->uniqueId();
}

auto vehicleKey(core::Device *device, dcc::VehicleAddress address)
{
    return vehicleGroupKey(device) + '/'_L1 + QString::number(address.value);
}

} // namespace

class VehicleControlView::Private : public core::PrivateObject<VehicleControlView>
{
public:
    using PrivateObject::PrivateObject;

    VehicleInfoModel *vehicleModel() const { return static_cast<VehicleInfoModel *>(vehicleView->model()); }

    auto centralLayout() const { return static_cast<QBoxLayout *>(q()->layout()); }
    int minimumWidthForHorizontalLayout() const;

    void setVariableControl(core::VariableControl *newControl);
    void setVehicleControl(core::VehicleControl *newControl);

    void showVehicle(dcc::VehicleAddress address);
    void updateFromVehicleInfo(const core::VehicleInfo &vehicleInfo);
    void restoreVehicleNames();

    void onSpeedDialValueChanged(int value);
    void onAddressBoxValueChanged();
    void onFindButtonClicked();
    void onEngageButtonToggled(bool checked);
    void onVehicleViewActivated(QModelIndex index);

    void onVehicleInfoChanged(const core::VehicleInfo &vehicleInfo);
    void onVehicleNameChanged(dcc::VehicleAddress address, QString name);

    QPointer<core::Device> device;
    QPointer<core::VariableControl> variableControl;
    QPointer<core::VehicleControl> vehicleControl;

    core::ConstPointer<SpeedDial> speedDial{q()};
    core::ConstPointer<SpinBox<dcc::VehicleAddress>> addressBox{q()};
    core::ConstPointer<QPushButton> findButton{tr("&Find"), q()};
    core::ConstPointer<QPushButton> engageButton{tr("D&rive"), q()};
    core::ConstPointer<QPushButton> emergencyStopButton{tr("S&top"), q()};
    core::ConstPointer<QLabel> reverseLabel{' '_L1 + value(gui::fontawesome::fasBackward), q()};
    core::ConstPointer<QLabel> forwardLabel{value(gui::fontawesome::fasForward) + ' '_L1, q()};
    core::ConstPointer<QLabel> conflictLabel{' '_L1 + value(gui::fontawesome::fasPersonCircleExclamation) + ' '_L1, q()};
    core::ConstPointer<QLabel> speedLabel{q()};
    const std::array<QPushButton *, 33> functionButtons = core::generateArray<QPushButton, 33>(q());

    core::ConstPointer<QTableView> vehicleView{q()};

    core::ConstPointer<QGraphicsOpacityEffect> reverseLabelOpacity{reverseLabel};
    core::ConstPointer<QPropertyAnimation> reverseLabelAnimation{reverseLabelOpacity, "opacity", reverseLabelOpacity};

    core::ConstPointer<QGraphicsOpacityEffect> forwardLabelOpacity{forwardLabel};
    core::ConstPointer<QPropertyAnimation> forwardLabelAnimation{forwardLabelOpacity, "opacity", forwardLabelOpacity};

    core::ConstPointer<QGraphicsOpacityEffect> conflictLabelOpacity{conflictLabel};
    core::ConstPointer<QPropertyAnimation> conflictLabelAnimation{conflictLabelOpacity, "opacity", conflictLabelOpacity};
};

void VehicleControlView::Private::onAddressBoxValueChanged()
{
    const auto address = addressBox->value();

    const auto index = vehicleModel()->findVehicle(address);
    updateFromVehicleInfo(index.data(VehicleInfoModel::VehicleInfoRole).value<core::VehicleInfo>());
    showVehicle(address);

    if (vehicleControl)
        vehicleControl->subscribe(address, core::VehicleControl::PrimarySubscription);

    if (variableControl) {
        auto nameVariables = range(dcc::VariableSpace::RailComPlusName).toList(dcc::extendedVariable);

#if 0
        variableControl->readExtendedVariables(address, std::move(nameVariables),
                                               [address](auto /*variable*/, auto value) {
            qInfo() << address << static_cast<char>(value);
            return core::Continuation::Abort;
//            return value ? core::Continuation::Proceed : core::Continuation::Abort;
        });
#endif
    }

    emit q()->currentVehicleChanged(address);
}

void VehicleControlView::Private::onFindButtonClicked()
{
#if 0
    driveControlFrame->setEnabled(false); // FIXME: do this via Programmer::state()

    // FIXME: maybe preserve current power mode
    programmer->powerOn(PowerSettings{PowerSettings::Programming}, [this](auto result) {
        if (result != LokProgrammer::Result::Success)
            return;

        programmer->readVariable(VehicleVariable::Configuration,
                                 [this](auto result, auto configuration) {
            if (result != LokProgrammer::Result::Success)
                return;

            qInfo() << "CV29:" << configuration;

            if (const auto extendedAddressed = !!(configuration & 0x20)) {
                qInfo() << "W00t";
                programmer->readVariable(VehicleVariable::ExtendedAddressHigh,
                                         [this](auto result, auto addressHigh) {
                    if (result != LokProgrammer::Result::Success) {
                        qWarning() << "Could not read CV17:" << result;
                        return;
                    }

                    qInfo() << "CV17:" << addressHigh << result;

                    programmer->readVariable(VehicleVariable::ExtendedAddressLow,
                                             [this, addressHigh](auto result, auto addressLow) {
                        qInfo() << "CV18:" << addressLow << result;
                        if (result == LokProgrammer::Result::Success) {
                            const auto extendedAddress = ((addressHigh << 8) | addressLow) & 0x3fff;
                            qInfo() << "CV17/CV18" << extendedAddress;
                            driveAddressBox->setValue(extendedAddress);
                        }

                        driveControlFrame->setEnabled(true); // FIXME: do this via Programmer::state()
                    });
                });
            } else {
                programmer->readVariable(VehicleVariable::BasicAddress,
                                         [this](auto result, auto basicAddress) {
                    if (result == LokProgrammer::Result::Success)
                        driveAddressBox->setValue(basicAddress);

                    driveControlFrame->setEnabled(true); // FIXME: do this via Programmer::state()
                });
            }
        });
    });
#endif
}

int VehicleControlView::Private::minimumWidthForHorizontalLayout() const
{
    const auto layout = centralLayout();
    const auto margins = layout->contentsMargins();

    auto width = margins.left() + margins.right()
            + layout->spacing() * (layout->count() - 1);

    for (auto i = 0; i < layout->count(); ++i)
        width += layout->itemAt(i)->minimumSize().width();

    return width;
}

void VehicleControlView::Private::setVariableControl(core::VariableControl *newControl)
{
    if (const auto oldControl = std::exchange(variableControl, newControl); oldControl != newControl) {
        if (oldControl)
            oldControl->disconnect(this);

        if (newControl) {
        }
    }
}

void VehicleControlView::Private::setVehicleControl(core::VehicleControl *newControl)
{
    if (const auto oldControl = std::exchange(vehicleControl, newControl); oldControl != newControl) {
        if (oldControl)
            oldControl->disconnect(this);

        vehicleModel()->clear();

        if (newControl) {
            restoreVehicleNames();

            connect(newControl, &core::VehicleControl::vehicleInfoChanged, this, &Private::onVehicleInfoChanged);
            connect(newControl, &core::VehicleControl::vehicleNameChanged, this, &Private::onVehicleNameChanged);
        }
    }
}

void VehicleControlView::Private::onSpeedDialValueChanged(int value)
{
    if (value == 0) {
        speedLabel->setText(tr("off"));
    } else if (value >= +(speedDial->valueSteps() - 1)) {
        speedLabel->setText(tr("max"));
    } else if (value <= -(speedDial->valueSteps() - 1)) {
        speedLabel->setText(tr("max"));
    } else {
        const auto percent = 100 * abs(speedDial->value()) / (speedDial->valueSteps() - 1);
        speedLabel->setText(QString::number(percent) + '%'_L1);
    }

    reverseLabelAnimation->stop();
    reverseLabelAnimation->setEndValue(speedDial->bias() == SpeedDial::Bias::Negative ? 1 : 0.05);
    reverseLabelAnimation->start();

    forwardLabelAnimation->stop();
    forwardLabelAnimation->setEndValue(speedDial->bias() == SpeedDial::Bias::Positive ? 1 : 0.05);
    forwardLabelAnimation->start();

    if (vehicleControl) {
        const auto speed = qAbs(speedDial->value());
        const auto dccSpeed = dcc::Speed126{speed > 0 ? static_cast<quint8>(speed + 1) : 0_u8};
        const auto direction = speedDial->bias() == SpeedDial::Bias::Positive
                ? dcc::Direction::Forward : dcc::Direction::Reverse;

        vehicleControl->setSpeed(addressBox->value(), dccSpeed, direction); // FIXME: read CV29 to select proper speed mode
    }
}

void VehicleControlView::Private::onEngageButtonToggled(bool /*checked*/)
{
#if 0
    if (checked)
        driveControlTimer->start(500ms);
    else
        driveControlTimer->stop();
#endif
}

void VehicleControlView::Private::onVehicleViewActivated(QModelIndex index)
{
    index = index.siblingAtColumn(value(VehicleInfoModel::Column::Address));
    const auto address = index.data(VehicleInfoModel::DataRole).value<dcc::VehicleAddress>();
    addressBox->setValue(address);
}

void VehicleControlView::Private::showVehicle(dcc::VehicleAddress address)
{
    const auto index = vehicleModel()->findVehicle(address);

    if (index.isValid()) {
        vehicleView->setCurrentIndex(index);
        vehicleView->scrollTo(index, QTableView::ScrollHint::PositionAtCenter);
    }
}

void VehicleControlView::Private::updateFromVehicleInfo(const core::VehicleInfo &vehicleInfo)
{
    auto speed = core::dcc::speedCast<dcc::Speed126>(vehicleInfo.speed()).count();

    if (speed > 0)  // FIXME: emergency stop
        speed -= 1;

    switch (vehicleInfo.direction()) {
    case dcc::Direction::Forward:
        speedDial->setValue(+speed, SpeedDial::Bias::Positive);
        break;

    case dcc::Direction::Reverse:
        speedDial->setValue(-speed, SpeedDial::Bias::Negative);
        break;

    case dcc::Direction::Unknown:
        break;
    }

    for (size_t i = 0; i < functionButtons.size(); ++i) {
        const auto fn = static_cast<dcc::Function::value_type>(i);
        functionButtons[i]->setChecked(vehicleInfo.functionState(fn));
    }
}

void VehicleControlView::Private::restoreVehicleNames()
{
    vehicleModel()->clear();

    auto settings = QSettings{};
    settings.beginGroup(vehicleGroupKey(vehicleControl->device()));

    for (const auto &group: settings.childGroups()) {
        bool isNumber = false;
        const auto number = group.toInt(&isNumber);
        auto name = settings.value(group + s_settingsSubkeyName).toString();

        if (isNumber
                && !name.isEmpty()
                && core::Range<dcc::VehicleAddress>{}.contains(number)) {
            const auto address = static_cast<dcc::VehicleAddress::value_type>(number);
            vehicleModel()->updateVehicleName(address, std::move(name));

            if (address == addressBox->value())
                showVehicle(address);

            vehicleControl->subscribe(address);
        }
    }
}

void VehicleControlView::Private::onVehicleInfoChanged(const core::VehicleInfo &info)
{
    vehicleModel()->updateVehicleInfo(info);

    // FIXME: we might want separate signals for functions and speed
    if (info.address() == addressBox->value()) {
        updateFromVehicleInfo(info);

        if (info.address() == addressBox->value()
                && !vehicleView->currentIndex().isValid())
            showVehicle(info.address());
    }
}

void VehicleControlView::Private::onVehicleNameChanged(dcc::VehicleAddress address, QString name)
{
    vehicleModel()->updateVehicleName(address, name);
    QSettings{}.setValue(vehicleKey(vehicleControl->device(), address) + s_settingsSubkeyName, name);
}

VehicleControlView::VehicleControlView(QWidget *parent)
    : MainWindowView{parent}
    , d{new Private{this}}
{
    setEnabled(false);

    d->speedDial->setValueSteps(127);

    d->reverseLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    d->reverseLabel->setFont({gui::fontawesome::solidFontFamily(), 12});
    d->reverseLabel->setGraphicsEffect(d->reverseLabelOpacity);

    d->forwardLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    d->forwardLabel->setFont({gui::fontawesome::solidFontFamily(), 12});
    d->forwardLabel->setGraphicsEffect(d->forwardLabelOpacity);

    d->conflictLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    d->conflictLabel->setFont({gui::fontawesome::solidFontFamily(), 12});
    d->conflictLabel->setGraphicsEffect(d->conflictLabelOpacity);
    d->conflictLabel->setStyleSheet("color: orange"_L1);

    d->speedLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    d->addressBox->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    d->addressBox->setWrapping(true);

    d->engageButton->setCheckable(true);
    d->emergencyStopButton->setCheckable(true);

    // FIXME: const auto metrics = QFontMetrics{lightFunctionButton->font()};
    const auto functionButtonWidth = 32; //metrics.boundingRect(" F## "_L1).width();
    for (size_t i = 0; i < d->functionButtons.size(); ++i) {
        const auto button = d->functionButtons[i];

        if (i == 0) {
            button->setText(tr("Light"));
        } else {
            button->setText("F"_L1 + QString::number(i));
            button->setMinimumWidth(functionButtonWidth);
        }

        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);

        connect(button, &QPushButton::toggled, this, [this, i](bool checked) {
            const auto fn = static_cast<dcc::Function::value_type>(i);
            vehicleControl()->setFunction(d->addressBox->value(), fn, checked);
        });
    }

    d->vehicleView->setShowGrid(false);
    d->vehicleView->setMinimumWidth(400);
    d->vehicleView->setSelectionBehavior(QTableView::SelectionBehavior::SelectRows);
    d->vehicleView->verticalHeader()->hide();

    d->vehicleView->setModel(new VehicleInfoModel{d->vehicleView});

    d->vehicleView->horizontalHeader()->setHighlightSections(false);
    d->vehicleView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    d->vehicleView->horizontalHeader()->setSectionResizeMode(value(VehicleInfoModel::Column::Functions),
                                                             QHeaderView::ResizeMode::Stretch);

    const auto addressLayout = new QHBoxLayout;
    addressLayout->addWidget(d->addressBox, 1);
    addressLayout->addWidget(d->findButton);

    const auto controlLayout = new QGridLayout;
    controlLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    controlLayout->addWidget(d->speedDial,             0, 0, 5, 2, Qt::AlignHCenter | Qt::AlignBottom);
    controlLayout->addWidget(d->reverseLabel,          0, 0, 1, 1);
    controlLayout->addWidget(d->forwardLabel,          0, 1, 1, 1);
    controlLayout->addWidget(d->conflictLabel,         4, 0, 1, 1);
    controlLayout->addWidget(d->speedLabel,            4, 1, 1, 1);

    controlLayout->addLayout(addressLayout,            0, 2, 1, 2);
    controlLayout->addWidget(d->engageButton,          1, 2, 1, 2);
    controlLayout->addWidget(d->emergencyStopButton,   2, 2, 1, 2);
    controlLayout->addItem(new QSpacerItem{0, 4},      3, 2, 1, 2);
    controlLayout->addWidget(d->functionButtons[0],    4, 2, 1, 2);

    for (size_t i = 1; i < d->functionButtons.size(); ++i) {
        const auto row = 7 + static_cast<int>((i - 1) / 4);
        const auto column = static_cast<int>(i - 1) % 4;
        controlLayout->addWidget(d->functionButtons[i], row, column);
    }

    const auto layout = new QBoxLayout{QBoxLayout::TopToBottom, this};

    layout->addLayout(controlLayout);
    layout->addWidget(d->vehicleView, 1);

    connect(d->addressBox, &QSpinBox::valueChanged, d, &Private::onAddressBoxValueChanged);
    connect(d->speedDial, &SpeedDial::valueChanged, d, &Private::onSpeedDialValueChanged);
    connect(d->findButton, &QPushButton::clicked, d, &Private::onFindButtonClicked);
    connect(d->engageButton, &QPushButton::toggled, d, &Private::onEngageButtonToggled);
    connect(d->vehicleView, &QTableView::activated, d, &Private::onVehicleViewActivated);

    d->findButton->setEnabled(false); // FIXME: Implement this button
    d->engageButton->setEnabled(false); // FIXME: Implement this button
    d->emergencyStopButton->setEnabled(false); // FIXME: Implement this button

    d->onSpeedDialValueChanged(d->speedDial->value());
}

DeviceFilter VehicleControlView::deviceFilter() const
{
    return DeviceFilter::require<core::VehicleControl>();
}

void VehicleControlView::updateControls(core::Device *newDevice)
{
    setVariableControl(newDevice ? newDevice->variableControl() : nullptr);
    setVehicleControl(newDevice ? newDevice->vehicleControl() : nullptr);
}

void VehicleControlView::setVariableControl(core::VariableControl *variableControl)
{
    const auto guard = core::propertyGuard(this, &VehicleControlView::variableControl,
                                           &VehicleControlView::variableControlChanged);

    d->setVariableControl(variableControl);
}

core::VariableControl *VehicleControlView::variableControl() const
{
    return d->variableControl;
}

void VehicleControlView::setVehicleControl(core::VehicleControl *vehicleControl)
{
    const auto guard = core::propertyGuard(this, &VehicleControlView::vehicleControl,
                                           &VehicleControlView::vehicleControlChanged);

    d->setVehicleControl(vehicleControl);
    setEnabled(!d->vehicleControl.isNull());
}

core::VehicleControl *VehicleControlView::vehicleControl() const
{
    return d->vehicleControl;
}

void VehicleControlView::setCurrentVehicle(core::dcc::VehicleAddress address)
{
    d->addressBox->setValue(address);
}

dcc::VehicleAddress VehicleControlView::currentVehicle() const
{
    return d->addressBox->value();
}

void VehicleControlView::resizeEvent(QResizeEvent *event)
{
    if (event->size().width() > d->minimumWidthForHorizontalLayout())
        d->centralLayout()->setDirection(QBoxLayout::LeftToRight);
    else
        d->centralLayout()->setDirection(QBoxLayout::TopToBottom);

    QWidget::resizeEvent(event);
}

} // namespace lmrs::studio

#include "moc_vehiclecontrolview.cpp"
