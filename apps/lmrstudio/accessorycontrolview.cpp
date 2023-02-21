#include "accessorycontrolview.h"
#include "accessorytablemodel.h"
#include "detectorinfotablemodel.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/propertyguard.h>

#include <lmrs/gui/fontawesome.h>

#include <lmrs/widgets/spinbox.h>

#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>

namespace lmrs::studio {

namespace dcc = core::dcc;

using widgets::SpinBox;

class AccessoryFilterModel : public QSortFilterProxyModel
{
public:
    using RowType = AccessoryTableModel::RowType;
    using QSortFilterProxyModel::QSortFilterProxyModel;

    explicit AccessoryFilterModel(RowType rowType, AccessoryTableModel *parent)
        : QSortFilterProxyModel{parent}
        , m_rowType{rowType}
    {
        setSourceModel(parent);
    }

    void setRowType(RowType rowType) { m_rowType = rowType; invalidateRowsFilter(); }
    RowType rowType() const { return m_rowType; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
    {
        const auto sourceIndex = sourceModel()->index(sourceRow, AccessoryTableModel::TypeColumn, sourceParent);
        return qvariant_cast<RowType>(sourceIndex.data(Qt::UserRole)) == m_rowType;
    }

private:
    RowType m_rowType = RowType::Invalid;
};

class AccessoryControlView::Private : public core::PrivateObject<AccessoryControlView>
{
public:
    enum class AccessoryType {
        SimpleTurnout,
        AdvancedTurnout,
        AdvancedSignal,
    };

    using PrivateObject::PrivateObject;

    void onAccessoryInfoChanged(core::AccessoryInfo info);
    void onTurnoutInfoChanged(core::TurnoutInfo info);

    void onAddressChanged();
    void onAccessoryTypeToggled(int id, bool checked);
    void onDirectionClicked(int id);

    void onSignalIndexChanged(int index);
    void onSignalValueChanged(int value);
    void onSignalButtonClicked();

    void onEmergencyStopButtonClicked();

    QPointer<core::Device> device;

    core::ConstPointer<QLabel> addressLabel{tr("&Address:"), q()};
    core::ConstPointer<SpinBox<dcc::AccessoryAddress>> addressBox{q()};

    core::ConstPointer<QButtonGroup> accessoryTypeGroup{q()};
    core::ConstPointer<QPushButton> simpleTurnoutButton{tr("&Basic Turnout"), q()};
    core::ConstPointer<QPushButton> advancedTurnoutButton{tr("Advanced &Turnout"), q()};
    core::ConstPointer<QPushButton> advancedSignalButton{tr("Ad&vanced Signal"), q()};

    core::ConstPointer<QButtonGroup> directionGroup{q()};

    core::ConstPointer<QHBoxLayout> directionsLayout;
    core::ConstPointer<QLabel> directionLabel{tr("Direction:"), q()};
    core::ConstPointer<QPushButton> straightButton{icon(gui::fontawesome::fasSquareCaretUp, Qt::darkGreen), tr("St&raight"), q()};
    core::ConstPointer<QPushButton> branchedButton{icon(gui::fontawesome::fasSquareArrowUpRight, Qt::darkRed), tr("&Branched"), q()};

    core::ConstPointer<QHBoxLayout> durationsLayout;
    core::ConstPointer<QLabel> durationLabel{tr("&Duration:"), q()};
    core::ConstPointer<QDoubleSpinBox> durationBox{q()};

    core::ConstPointer<QHBoxLayout> signalLayout;
    core::ConstPointer<QLabel> signalLabel{tr("S&ignal:"), q()};
    core::ConstPointer<QSpinBox> signalBox{q()};
    core::ConstPointer<QComboBox> signalCombo{q()};
    core::ConstPointer<QPushButton> signalButton{tr("&Send"), q()};

    core::ConstPointer<QPushButton> emergencyStopButton{icon(gui::fontawesome::fasHand), tr("Sto&p"), q()};

    core::ConstPointer<AccessoryTableModel> accessoryInfoModel{q()};
    core::ConstPointer<QTableView> accessoryInfoView{q()};
    core::ConstPointer<QTableView> turnoutInfoView{q()};

    core::ConstPointer<DetectorInfoTableModel> detectorInfoModel{q()};
    core::ConstPointer<QTableView> detectorInfoView{q()};
};

void AccessoryControlView::Private::onAccessoryInfoChanged(core::AccessoryInfo info)
{
    qCInfo(logger()) << __func__ << info.address() << info.state();

    if (info.address() == addressBox->value())
        signalBox->setValue(info.state());
}

void AccessoryControlView::Private::onTurnoutInfoChanged(core::TurnoutInfo info)
{
    qCInfo(logger()) << __func__ << info.address() << info.state();

    if (info.address() == addressBox->value()) {
        straightButton->setChecked(info.state() == dcc::TurnoutState::Straight);
        branchedButton->setChecked(info.state() == dcc::TurnoutState::Branched);
    }
}

void AccessoryControlView::Private::onAddressChanged()
{
    if (const auto accessoryControl = q()->accessoryControl()) {
        accessoryControl->requestAccessoryInfo(addressBox->value(), [this](core::AccessoryInfo info) {
            onAccessoryInfoChanged(std::move(info));
        });

        accessoryControl->requestTurnoutInfo(addressBox->value(), [this](core::TurnoutInfo info) {
            onTurnoutInfoChanged(std::move(info));
        });
    }
}

void AccessoryControlView::Private::onAccessoryTypeToggled(int id, bool checked)
{
    if (!checked)
        return;

    qInfo() << q()->size() << q()->minimumSize();

    const auto type = static_cast<AccessoryType>(id);
    const auto layout = static_cast<QFormLayout *>(q()->layout());

    const auto rowOf = [layout](QLayout *child) {
        auto row = -1;
        layout->getLayoutPosition(child, &row, nullptr);
        return row;
    };

    layout->setRowVisible(rowOf(directionsLayout), type != AccessoryType::AdvancedSignal);
    layout->setRowVisible(rowOf(durationsLayout), type == AccessoryType::AdvancedTurnout);
    layout->setRowVisible(rowOf(signalLayout), type == AccessoryType::AdvancedSignal);

    emergencyStopButton->setVisible(type != AccessoryType::SimpleTurnout);
}

void AccessoryControlView::Private::onDirectionClicked(int id)
{
    const auto state = static_cast<dcc::TurnoutState>(id);
    const auto duration = std::chrono::milliseconds{qRound(durationBox->value() * 1000)};

    qCInfo(logger()) << "turnout click:" << state;

    if (const auto accessoryControl = q()->accessoryControl()) {
        switch (static_cast<AccessoryType>(accessoryTypeGroup->checkedId())) {
        case AccessoryType::SimpleTurnout:
            accessoryControl->setTurnoutState(addressBox->value(), state);
            break;

        case AccessoryType::AdvancedTurnout:
            accessoryControl->setTurnoutState(addressBox->value(), state, duration);
            break;

        case AccessoryType::AdvancedSignal:
            Q_UNREACHABLE();
            break;
        }
    }
}

void AccessoryControlView::Private::onEmergencyStopButtonClicked()
{
    if (const auto accessoryControl = q()->accessoryControl())
        accessoryControl->requestEmergencyStop();
}

void AccessoryControlView::Private::onSignalIndexChanged(int index)
{
    if (const auto data = signalCombo->itemData(index); data.isValid())
        signalBox->setValue(qvariant_cast<quint8>(data));
}

void AccessoryControlView::Private::onSignalValueChanged(int value)
{
    const auto index = signalCombo->findData(value);
    signalCombo->setCurrentIndex(index);
}

void AccessoryControlView::Private::onSignalButtonClicked()
{
    if (const auto accessoryControl = q()->accessoryControl())
        accessoryControl->setAccessoryState(addressBox->value(), static_cast<quint8>(signalBox->value()));
}

AccessoryControlView::AccessoryControlView(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    setEnabled(false);

    d->addressLabel->setMinimumWidth(50);
    d->addressBox->setMinimumWidth(100);
    d->addressBox->setRange(1, 2048);

    d->simpleTurnoutButton->setCheckable(true);
    d->simpleTurnoutButton->setFocusPolicy(Qt::FocusPolicy::TabFocus);

    d->advancedTurnoutButton->setCheckable(true);
    d->advancedTurnoutButton->setFocusPolicy(Qt::FocusPolicy::TabFocus);

    d->advancedSignalButton->setCheckable(true);
    d->advancedSignalButton->setFocusPolicy(Qt::FocusPolicy::TabFocus);

    d->accessoryTypeGroup->addButton(d->simpleTurnoutButton, core::value(Private::AccessoryType::SimpleTurnout));
    d->accessoryTypeGroup->addButton(d->advancedTurnoutButton, core::value(Private::AccessoryType::AdvancedTurnout));
    d->accessoryTypeGroup->addButton(d->advancedSignalButton, core::value(Private::AccessoryType::AdvancedSignal));

    d->straightButton->setCheckable(true);
    d->straightButton->setFocusPolicy(Qt::FocusPolicy::TabFocus);

    d->branchedButton->setCheckable(true);
    d->branchedButton->setFocusPolicy(Qt::FocusPolicy::TabFocus);

    d->directionGroup->addButton(d->straightButton, core::value(dcc::TurnoutState::Straight));
    d->directionGroup->addButton(d->branchedButton, core::value(dcc::TurnoutState::Branched));

    d->addressLabel->setBuddy(d->addressBox);
    d->durationLabel->setBuddy(d->durationBox);
    d->signalLabel->setBuddy(d->signalBox);

    d->durationBox->setRange(0, 12.7);
    d->durationBox->setDecimals(1);
    d->durationBox->setSingleStep(0.1);
    d->durationBox->setSpecialValueText(tr("off"));
    d->durationBox->setSuffix(tr(" s"));
    d->durationBox->setValue(0.2);

    d->signalBox->setMinimumWidth(100);
    d->signalBox->setDisplayIntegerBase(16);
    d->signalBox->setPrefix("0x"_L1);
    d->signalBox->setRange(0, 255);

    const auto setupTableView = [](QTableView *view, QAbstractItemModel *model) {
        view->setModel(model);

        if (dynamic_cast<AccessoryFilterModel *>(model))
            view->hideColumn(AccessoryTableModel::TypeColumn);

        view->setSelectionBehavior(QTableView::SelectionBehavior::SelectRows);
        view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
        view->horizontalHeader()->setStretchLastSection(true);
        view->verticalHeader()->setDefaultSectionSize(9);
        view->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Fixed);
        view->verticalHeader()->hide();
    };

    using RowType = AccessoryTableModel::RowType;
    setupTableView(d->accessoryInfoView, new AccessoryFilterModel{RowType::Accessory, d->accessoryInfoModel});
    setupTableView(d->turnoutInfoView, new AccessoryFilterModel{RowType::Turnout, d->accessoryInfoModel});
    setupTableView(d->detectorInfoView, d->detectorInfoModel);

    d->accessoryInfoView->setMinimumWidth(200);
    d->turnoutInfoView->setMinimumWidth(200);
    d->detectorInfoView->setMinimumWidth(400);

    for (const auto &entry: QMetaTypeId<core::AccessoryInfo::CommonSignal>{}) {
        const auto value = core::value(entry.value());
        auto label = tr("%1 (0x%2)").arg(QString::fromLatin1(entry.key()), QString::number(value, 16));
        d->signalCombo->addItem(label, value);
    }

    const auto addressLayout = new QHBoxLayout;
    addressLayout->addWidget(d->addressBox);
    addressLayout->addWidget(d->simpleTurnoutButton);
    addressLayout->addWidget(d->advancedTurnoutButton);
    addressLayout->addWidget(d->advancedSignalButton);
    addressLayout->addStretch();
    addressLayout->addWidget(d->emergencyStopButton);

    d->directionsLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d->directionsLayout->addWidget(d->straightButton);
    d->directionsLayout->addWidget(d->branchedButton);
    d->directionsLayout->addStretch();

    d->durationsLayout->addWidget(d->durationBox);
    d->durationsLayout->addStretch();

    d->signalLayout->addWidget(d->signalBox);
    d->signalLayout->addWidget(d->signalCombo);
    d->signalLayout->addWidget(d->signalButton);
    d->signalLayout->addStretch();

    const auto modelColumnsLayout = new QHBoxLayout;
    modelColumnsLayout->addWidget(d->turnoutInfoView);
    modelColumnsLayout->addWidget(d->accessoryInfoView);
    modelColumnsLayout->addWidget(d->detectorInfoView, 1);

    const auto layout = new QFormLayout{this};
    layout->addRow(d->addressLabel, addressLayout);
    layout->addRow(d->directionLabel, d->directionsLayout);
    layout->addRow(d->durationLabel, d->durationsLayout);
    layout->addRow(d->signalLabel, d->signalLayout);
    layout->addRow(modelColumnsLayout);

    connect(d->addressBox, &QSpinBox::valueChanged, d, &Private::onAddressChanged);
    connect(d->accessoryTypeGroup, &QButtonGroup::idToggled, d, &Private::onAccessoryTypeToggled);
    connect(d->directionGroup, &QButtonGroup::idClicked, d, &Private::onDirectionClicked);

    connect(d->signalBox, &QSpinBox::valueChanged, d, &Private::onSignalValueChanged);
    connect(d->signalCombo, &QComboBox::currentIndexChanged, d, &Private::onSignalIndexChanged);
    connect(d->signalButton, &QPushButton::clicked, d, &Private::onSignalButtonClicked);

    connect(d->emergencyStopButton, &QPushButton::clicked, d, &Private::onEmergencyStopButtonClicked);

    connect(d->accessoryInfoModel, &AccessoryTableModel::accessoryControlChanged,
            this, &AccessoryControlView::accessoryControlChanged);
    connect(d->detectorInfoModel, &DetectorInfoTableModel::detectorControlChanged,
            this, &AccessoryControlView::detectorControlChanged);

    d->simpleTurnoutButton->setChecked(true);
}

AccessoryControlView::~AccessoryControlView()
{
    delete d;
}

void AccessoryControlView::setDevice(core::Device *device)
{
    setAccessoryControl(device->accessoryControl());
    setDetectorControl(device->detectorControl());
}

core::Device *AccessoryControlView::device() const
{
    if (const auto control = accessoryControl())
        return control->device();
    if (const auto control = detectorControl())
        return control->device();

    return nullptr;
}

void AccessoryControlView::setAccessoryControl(core::AccessoryControl *newControl)
{
    if (const auto oldControl = d->accessoryInfoModel->accessoryControl(); oldControl != newControl) {
        if (oldControl)
            oldControl->disconnect(this);

        d->accessoryInfoModel->setAccessoryControl(newControl);

        if (newControl) {
            connect(newControl, &core::AccessoryControl::accessoryInfoChanged, d, &Private::onAccessoryInfoChanged);
            connect(newControl, &core::AccessoryControl::turnoutInfoChanged, d, &Private::onTurnoutInfoChanged);
        }
    }

    setEnabled(accessoryControl());
}

core::AccessoryControl *AccessoryControlView::accessoryControl() const
{
    return d->accessoryInfoModel->accessoryControl();
}

void AccessoryControlView::setDetectorControl(core::DetectorControl *newControl)
{
    d->detectorInfoModel->setDetectorControl(newControl);
    d->detectorInfoView->setVisible(detectorControl());
}

core::DetectorControl *AccessoryControlView::detectorControl() const
{
    return d->detectorInfoModel->detectorControl();
}

void AccessoryControlView::setCurrentAccessory(core::dcc::AccessoryAddress address)
{
    d->addressBox->setValue(address);
}

core::dcc::AccessoryAddress AccessoryControlView::currentAccessory() const
{
    return d->addressBox->value();
}

} // namespace lmrs::studio
