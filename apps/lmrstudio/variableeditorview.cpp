#include "variableeditorview.h"

#include "deviceconnectionview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/dccconstants.h>
#include <lmrs/core/decoderinfo.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/variabletreemodel.h>

#include <lmrs/gui/fontawesome.h>

#include <lmrs/widgets/spinbox.h>
#include <lmrs/widgets/statusbar.h>

#include <QApplication>
#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QHeaderView>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QTreeView>

namespace lmrs::studio {

using namespace core;
using namespace widgets;

class VariableEditorView::Private : public core::PrivateObject<VariableEditorView>
{
public:
    using PrivateObject::PrivateObject;

    auto variableModel() const { return checked_cast<VariableModel *>(readingsView->model()); }

    auto centralLayout() const { return static_cast<QBoxLayout *>(q()->layout()); }
    int minimumWidthForHorizontalLayout() const;

    void showStatus(QString message);
    void updateExtendedPageIndex();
    void updateSusiPageIndex();

    void onAddressBoxChanged();
    void onVariableBoxChanged();
    void onExtendedPageBoxChanged();
    void onPageIndexUpperLowerBoxChanged();
    void onSusiPageBoxChanged();
    void onValueType8BitToggled(bool checked);
    void onValueType16BitToggled(bool checked);
    void onValueType32BitToggled(bool checked);
    void onReadVariableButtonClicked();
    void onWriteVariableButtonClicked();
    void onReadAllVariablesButtonClicked();
    void onIdentifyButtonClicked();
    void onClearReadingsButtonClicked();

    QPointer<VariableControl> variableControl;
    dcc::VehicleAddress currentVehicle = 3;

    ConstPointer<SpinBox<dcc::VehicleAddress>> addressBox{q()};
    ConstPointer<SpinBox<dcc::VariableIndex>> variableBox{q()};
    ConstPointer<QStackedWidget> pageModeStack{q()};

    ConstPointer<QLabel> regularPage{tr("Regular variable without paging"), pageModeStack};

    ConstPointer<QWidget> extendedPage{pageModeStack};
    ConstPointer<SpinBox<dcc::ExtendedPageIndex>> extendedPageBox{extendedPage};
    ConstPointer<SpinBox<dcc::VariableValue>> extendedPageUpperBox{extendedPage};
    ConstPointer<SpinBox<dcc::VariableValue>> extendedPageLowerBox{extendedPage};

    ConstPointer<QWidget> susiPage{pageModeStack};
    ConstPointer<SpinBox<dcc::SusiPageIndex>> susiPageBox{susiPage};

    ConstPointer<MultiBaseSpinBox> valueBox{q()};
    ConstPointer<QRadioButton> valueType8BitButton{tr("&8 Bit"), q()};
    ConstPointer<QRadioButton> valueType16BitButton{tr("&16 Bit"), q()};
    ConstPointer<QRadioButton> valueType32BitButton{tr("&32 Bit"), q()};

    ConstPointer<QPushButton> lockVariablesButton{icon(gui::fontawesome::fasLockOpen), QString{}, q()};
    ConstPointer<QPushButton> readVariableButton{tr("&Read CV"), q()};
    ConstPointer<QPushButton> writeVariableButton{tr("&Write CV"), q()};
    ConstPointer<QPushButton> readAllVariablesButton{tr("Read &All"), q()};
    ConstPointer<QPushButton> identifyButton{tr("&Identify"), q()};
    ConstPointer<QPushButton> clearReadingsButton{tr("&Clear"), q()};

    ConstPointer<QTreeView> readingsView{q()};

    QPointer<widgets::StatusBar> statusBar;
};

int VariableEditorView::Private::minimumWidthForHorizontalLayout() const
{
    const auto layout = centralLayout();
    const auto margins = layout->contentsMargins();

    auto width = margins.left() + margins.right()
            + layout->spacing() * (layout->count() - 1);

    for (auto i = 0; i < layout->count(); ++i)
        width += layout->itemAt(i)->minimumSize().width();

    return width;
}

void VariableEditorView::Private::showStatus(QString message)
{
    if (statusBar)
        statusBar->showTemporaryMessage(std::move(message));
}

void VariableEditorView::Private::updateExtendedPageIndex()
{
    variableControl->readExtendedVariables(q()->currentVehicle(),
                                           {value(dcc::VehicleVariable::ExtendedPageIndexHigh),
                                            value(dcc::VehicleVariable::ExtendedPageIndexLow)},
                                           [this](auto variableIndex, auto result) {
        if (result.failed())
            return Continuation::Retry;

        if (variableIndex == value(dcc::VehicleVariable::ExtendedPageIndexHigh))
            extendedPageUpperBox->setValue(result.value);
        else if (variableIndex == value(dcc::VehicleVariable::ExtendedPageIndexLow))
            extendedPageLowerBox->setValue(result.value);

        variableModel()->updateVariable(variableIndex, result.value);
        return Continuation::Proceed;
    });
}

void VariableEditorView::Private::updateSusiPageIndex()
{
    static const auto variable = variableIndex(dcc::VehicleVariable::SusiBankIndex);
    variableControl->readVariable(q()->currentVehicle(), variable, [this](auto result) {
        if (result.failed())
            return Continuation::Retry;

        susiPageBox->setValue(dcc::SusiPageIndex{result.value});
        variableModel()->updateVariable(variable.value, result.value);
        return Continuation::Proceed;
    });
}

void VariableEditorView::Private::onAddressBoxChanged()
{
    if (const auto address = addressBox->value()) {
        if (std::exchange(currentVehicle, address) != currentVehicle)
            emit q()->currentVehicleChanged(currentVehicle);
    }

    if (range(dcc::VariableSpace::Extended).contains(variableBox->value()))
        updateExtendedPageIndex();
    else if (range(dcc::VariableSpace::Susi).contains(variableBox->value()))
        updateSusiPageIndex();
}

void VariableEditorView::Private::onVariableBoxChanged()
{
    const auto currentWidgetGuard = propertyGuard(pageModeStack.get(), &QStackedWidget::currentWidget);
    auto extendedIndex = dcc::ExtendedVariableIndex{variableBox->value()};

    // apply extend page index if needed
    if (range(dcc::VariableSpace::Extended).contains(variableBox->value())) {
        const auto pageIndex = extendedPageBox->value();
        extendedIndex = dcc::extendedVariable(variableBox->value(), pageIndex);
        pageModeStack->setCurrentWidget(extendedPage);

        if (currentWidgetGuard.hasChanged())
            updateExtendedPageIndex();
    } else if (range(dcc::VariableSpace::Susi).contains(variableBox->value())) {
        const auto pageIndex = susiPageBox->value();
        extendedIndex = dcc::susiVariable(variableBox->value(), pageIndex);
        pageModeStack->setCurrentWidget(susiPage);

        if (currentWidgetGuard.hasChanged())
            updateSusiPageIndex();
    } else {
        pageModeStack->setCurrentWidget(regularPage);
    }

    auto suffix = dcc::fullVariableName(extendedIndex);

    if (!suffix.isEmpty())
        suffix = " ("_L1 + suffix + ")"_L1;

    variableBox->setSuffix(std::move(suffix));

    switch (dcc::variableType(extendedIndex)) {
    case dcc::VariableType::U8:
    case dcc::VariableType::UTF8:
        valueType8BitButton->setChecked(true);
        break;

    case dcc::VariableType::U16H:
    case dcc::VariableType::U16L:
        valueType16BitButton->setChecked(true);
        break;

    case dcc::VariableType::U32H:
    case dcc::VariableType::D32H:
        valueType32BitButton->setChecked(true);
        break;

    case dcc::VariableType::Invalid:
        break;
    }
}

void VariableEditorView::Private::onExtendedPageBoxChanged()
{
    const auto value = extendedPageBox->value();

    extendedPageUpperBox->setValue(static_cast<dcc::VariableValue::value_type>(255 & (value >> 8)));
    extendedPageLowerBox->setValue(static_cast<dcc::VariableValue::value_type>(255 & (value >> 0)));

    onVariableBoxChanged(); // FIXME: find better api
}

void VariableEditorView::Private::onPageIndexUpperLowerBoxChanged()
{
    const auto upper = extendedPageUpperBox->value();
    const auto lower = extendedPageLowerBox->value();

    extendedPageBox->setValue(static_cast<dcc::ExtendedPageIndex::value_type>((upper << 8) | lower));
}

void VariableEditorView::Private::onSusiPageBoxChanged()
{
    onVariableBoxChanged(); // FIXME: find better api
}

void VariableEditorView::Private::onValueType8BitToggled(bool checked)
{
    if (checked)
        valueBox->setMaximum(MaximumValue<quint8>);
}

void VariableEditorView::Private::onValueType16BitToggled(bool checked)
{
    if (checked)
        valueBox->setMaximum(MaximumValue<quint16>);
}

void VariableEditorView::Private::onValueType32BitToggled(bool checked)
{
    if (checked) {
        // FIXME: base widgets::SpinBox or QAbstractSpinBox to fully support 32 bit values
        if constexpr (sizeof(int) > 4) {
            valueBox->setMaximum(static_cast<int>(MaximumValue<quint32>));
        } else {
            valueBox->setMaximum(MaximumValue<qint32>);
        }
    }
}

void VariableEditorView::Private::onReadVariableButtonClicked()
{
#if 0
    variableFrame->setEnabled(false); // FIXME: do this via Programmer::state()
#endif

    const auto vehicle = q()->currentVehicle();
    const auto variable = q()->currentVariable();

    if (vehicle > 0) {
        showStatus(tr("Reading %1 of vehicle %2...").arg(variableString(variable), QString::number(vehicle)));
    } else {
        showStatus(tr("Reading %1 in direct mode...").arg(variableString(variable)));
    }

    variableControl->readExtendedVariable(vehicle, variable, [this, variable](auto result) {
        if (result.failed()) {
            showStatus(tr("Could not read variable, retrying..."));
            return Continuation::Retry;
        }

        showStatus(tr("Value %1 received for %2").arg(QString::number(result.value), variableString(variable)));
        variableModel()->updateVariable(variable, result.value);
        valueBox->setValue(result.value);

        return Continuation::Proceed;

#if 0
        variableFrame->setEnabled(true); // FIXME: do this via Programmer::state(), also handle timeout
#endif
    });
}

void VariableEditorView::Private::onWriteVariableButtonClicked()
{
#if 0
    variableFrame->setEnabled(false); // FIXME: do this via Programmer::state()
#endif

    const auto vehicle = q()->currentVehicle();
    const auto variable = q()->currentVariable();
    const auto value = static_cast<dcc::VariableValue::value_type>(valueBox->value()); // FIXME: handle 16 and 32 bit writes

    if (vehicle > 0) {
        showStatus(tr("Writing %1 of vehicle %2...").arg(variableString(variable), QString::number(vehicle)));
    } else {
        showStatus(tr("Writing %1 in direct mode...").arg(variableString(variable)));
    }

    variableControl->writeExtendedVariable(vehicle, variable, value, [this, variable](auto result) {
        if (result.failed()) {
            showStatus(tr("Could not write variable, retrying..."));
            return Continuation::Retry;
        }

        showStatus(tr("Value %1 written for %2").arg(QString::number(result.value), variableString(variable)));
        variableModel()->updateVariable(variable, result.value);
        valueBox->setValue(result.value);

        return Continuation::Proceed;
#if 0
        variableFrame->setEnabled(true); // FIXME: do this via Programmer::state(), also handle timeout
#endif
    });
}

void VariableEditorView::Private::onReadAllVariablesButtonClicked()
{
    // FIXME: only enable once identified

//    variableModel()->clear();
    variableControl->readExtendedVariables(addressBox->value(),
                                           variableModel()->knownVariables(),
                                           [this](auto variable, auto result) {
        // FIXME: share this callback with onReadAllVariablesButtonClicked() at least,
        // but maybe also with onReadVariableButtonClicked() amd onWriteVariableButtonClicked()

        if (result.failed()) {
            showStatus(tr("Could not read variable, retrying..."));
            return Continuation::Retry;
        }

        variableModel()->updateVariable(variable, result.value);
        return Continuation::Proceed;
    });
}

void VariableEditorView::Private::onIdentifyButtonClicked()
{
    variableModel()->clear();

    // FIXME: switch to POM mode, once possible

    variableControl->readExtendedVariables(addressBox->value(),
                                           dcc::DecoderInfo{dcc::DecoderInfo::Identity}.variableIds(),
                                           [this](auto variable, auto result) {
        // FIXME: share this callback with onReadAllVariablesButtonClicked() at least,
        // but maybe also with onReadVariableButtonClicked() amd onWriteVariableButtonClicked()

        if (result.failed()) {
            showStatus(tr("Could not read variable, retrying..."));
            return Continuation::Retry;
        }

        variableModel()->updateVariable(variable, result.value);
        return Continuation::Proceed;
    });
}

void VariableEditorView::Private::onClearReadingsButtonClicked()
{
    variableModel()->clear();
}

VariableEditorView::VariableEditorView(QWidget *parent)
    : MainWindowView{parent}
    , d{new Private{this}}
{
    setEnabled(false);

    d->addressBox->setMinimum(0); // an address of zero indicates direct mode
    d->addressBox->setSpecialValueText(tr("Direct Mode"));
    d->addressBox->setValue(0);

    d->variableBox->setPrefix(tr("CV\u202f"));
    d->variableBox->setGroupSeparatorShown(true);
    d->variableBox->setWrapping(true);

    d->pageModeStack->addWidget(d->regularPage);
    d->pageModeStack->addWidget(d->extendedPage);
    d->pageModeStack->addWidget(d->susiPage);

    d->regularPage->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    const auto extendPageLogicalLabel = new QLabel{tr("E&xtended:"), d->extendedPage};
    extendPageLogicalLabel->setBuddy(d->extendedPageBox);

    const auto extendPageRawLabel = new QLabel{tr("CV&31/32:"), d->extendedPage};
    extendPageRawLabel->setBuddy(d->extendedPageUpperBox);

    d->extendedPageBox->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->extendedPageBox->setGroupSeparatorShown(true);
    d->extendedPageBox->setPrefix(tr("Bank="));
    d->extendedPageBox->setWrapping(true);

    d->extendedPageUpperBox->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->extendedPageUpperBox->setPrefix(tr("CV31="));
    d->extendedPageUpperBox->setWrapping(true);

    d->extendedPageLowerBox->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->extendedPageLowerBox->setPrefix(tr("CV32="));
    d->extendedPageLowerBox->setWrapping(true);

    const auto susiPageLabel = new QLabel{tr("&SUSI:"), d->susiPage};
    susiPageLabel->setBuddy(d->susiPageBox);

    d->susiPageBox->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->susiPageBox->setPrefix(tr("Bank="));
    d->susiPageBox->setWrapping(true);

    d->valueBox->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    d->lockVariablesButton->setEnabled(false); // FIXME implement this button
    d->readVariableButton->setFocusPolicy(Qt::NoFocus);
    d->writeVariableButton->setFocusPolicy(Qt::NoFocus);

    d->readingsView->setModel(new VariableModel{d->readingsView});
    d->readingsView->header()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    d->readingsView->header()->setSectionResizeMode(VariableModel::Column::Description,
                                                    QHeaderView::ResizeMode::Stretch);

    const auto extendedPageLayout = new QHBoxLayout{d->extendedPage}; // maybe we can make this a flow layout

    extendedPageLayout->addWidget(extendPageLogicalLabel);
    extendedPageLayout->addStretch(2);
    extendedPageLayout->addWidget(d->extendedPageUpperBox, 1);
    extendedPageLayout->addWidget(d->extendedPageLowerBox, 1);
    extendedPageLayout->addSpacing(12);
    extendedPageLayout->addWidget(d->extendedPageBox, 1);
    extendedPageLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    extendedPageLayout->setContentsMargins({});

    extendPageRawLabel->hide();

    const auto susiPageLayout = new QHBoxLayout{d->susiPage};

    susiPageLayout->addWidget(susiPageLabel, 2);
    susiPageLayout->addWidget(d->susiPageBox, 1);
    susiPageLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    susiPageLayout->setContentsMargins({});

    const auto valueTypeLayout = new QHBoxLayout;

    valueTypeLayout->addWidget(new QLabel{tr("Value Type:"), this}, 1);
    valueTypeLayout->addWidget(d->valueType8BitButton);
    valueTypeLayout->addWidget(d->valueType16BitButton);
    valueTypeLayout->addWidget(d->valueType32BitButton);

    const auto upperButtonLayout = new QHBoxLayout;

    upperButtonLayout->addStretch(1);
    upperButtonLayout->addWidget(d->lockVariablesButton);
    upperButtonLayout->addWidget(d->readVariableButton);
    upperButtonLayout->addWidget(d->writeVariableButton);

    const auto lowerButtonLayout = new QHBoxLayout;

    lowerButtonLayout->addWidget(d->readAllVariablesButton);
    lowerButtonLayout->addWidget(d->identifyButton);
    lowerButtonLayout->addStretch(1);
    lowerButtonLayout->addSpacing(12);
    lowerButtonLayout->addWidget(d->clearReadingsButton);

    const auto controlLayout = new QFormLayout;

    controlLayout->addRow(tr("&Address:"), d->addressBox);
    controlLayout->addRow(tr("Var&iable:"), d->variableBox);
    controlLayout->addRow(nullptr, d->pageModeStack);
    controlLayout->addItem(new QSpacerItem{0, 4});
    controlLayout->addRow(tr("Va&lue:"), d->valueBox);
    controlLayout->addRow(nullptr, valueTypeLayout);
    controlLayout->addItem(new QSpacerItem{0, 8});
    controlLayout->addRow(upperButtonLayout);

    const auto readingsLayout = new QVBoxLayout;

    readingsLayout->addWidget(d->readingsView, 1);
    readingsLayout->addLayout(lowerButtonLayout);

    const auto layout = new QBoxLayout{QBoxLayout::TopToBottom, this};

    layout->addLayout(controlLayout);
    layout->addLayout(readingsLayout, 1);

    connect(d->addressBox, &QSpinBox::valueChanged, d, &Private::onAddressBoxChanged);
    connect(d->variableBox, &QSpinBox::valueChanged, d, &Private::onVariableBoxChanged);
    connect(d->extendedPageBox, &QSpinBox::valueChanged, d, &Private::onExtendedPageBoxChanged);
    connect(d->extendedPageUpperBox, &QSpinBox::valueChanged, d, &Private::onPageIndexUpperLowerBoxChanged);
    connect(d->extendedPageLowerBox, &QSpinBox::valueChanged, d, &Private::onPageIndexUpperLowerBoxChanged);
    connect(d->susiPageBox, &QSpinBox::valueChanged, d, &Private::onSusiPageBoxChanged);
    connect(d->valueType8BitButton, &QRadioButton::toggled, d, &Private::onValueType8BitToggled);
    connect(d->valueType16BitButton, &QRadioButton::toggled, d, &Private::onValueType16BitToggled);
    connect(d->valueType32BitButton, &QRadioButton::toggled, d, &Private::onValueType32BitToggled);
    connect(d->readVariableButton, &QPushButton::clicked, d, &Private::onReadVariableButtonClicked);
    connect(d->writeVariableButton, &QPushButton::clicked, d, &Private::onWriteVariableButtonClicked);
    connect(d->readAllVariablesButton, &QPushButton::clicked, d, &Private::onReadAllVariablesButtonClicked);
    connect(d->identifyButton, &QPushButton::clicked, d, &Private::onIdentifyButtonClicked);
    connect(d->clearReadingsButton, &QPushButton::clicked, d, &Private::onClearReadingsButtonClicked);

    d->onVariableBoxChanged();
}

VariableEditorView::~VariableEditorView()
{
    delete d;
}

DeviceFilter VariableEditorView::deviceFilter() const
{
    return DeviceFilter::require<VariableControl>();
}

void VariableEditorView::setDevice(core::Device *newDevice)
{
    setVariableControl(newDevice ? newDevice->variableControl() : nullptr);
}

Device *VariableEditorView::device() const
{
    return variableControl()->device();
}

void VariableEditorView::setVariableControl(VariableControl *variableControl)
{
    const auto guard = propertyGuard(this, &VariableEditorView::variableControl,
                                     &VariableEditorView::variableControlChanged);

    d->variableControl = variableControl;

    if (d->variableControl) {
        const auto features = d->variableControl->features();

        if (features & VariableControl::Feature::ProgrammingOnMain) {
            d->addressBox->setMinimum(0);
            d->addressBox->setValue(0);
        } else {
            d->addressBox->setMinimum(1);
        }

        if (features & VariableControl::Feature::ProgrammingOnMain) {
            d->addressBox->setValue(d->currentVehicle);
            d->addressBox->setEnabled(true);
        } else {
            d->addressBox->setEnabled(false);
        }
    }

    setEnabled(!d->variableControl.isNull());
}

VariableControl *VariableEditorView::variableControl() const
{
    return d->variableControl;
}

void VariableEditorView::setCurrentVehicle(core::dcc::VehicleAddress address)
{
    // FIXME: does this really make sense?
    if (!d->addressBox->isEnabled())
        address = 0;

    if (std::exchange(d->currentVehicle, address) != d->currentVehicle) {
        if (d->addressBox->isEnabled())
            d->addressBox->setValue(d->currentVehicle);

        emit currentVehicleChanged(d->currentVehicle);
    }
}

dcc::VehicleAddress VariableEditorView::currentVehicle() const
{
    return d->currentVehicle;
}

dcc::ExtendedVariableIndex VariableEditorView::currentVariable() const
{
    const auto variableIndex = d->variableBox->value();

    if (range(dcc::VariableSpace::Extended).contains(variableIndex))
        return dcc::extendedVariable(variableIndex, d->extendedPageBox->value());
    else if (range(dcc::VariableSpace::Susi).contains(variableIndex))
        return dcc::susiVariable(variableIndex, d->susiPageBox->value());

    return static_cast<dcc::ExtendedVariableIndex>(value(variableIndex));
}

void VariableEditorView::setStatusBar(widgets::StatusBar *statusBar)
{
    d->statusBar = statusBar;
}

StatusBar *VariableEditorView::statusBar() const
{
    return d->statusBar;
}

void VariableEditorView::resizeEvent(QResizeEvent *event)
{
    if (event->size().width() > d->minimumWidthForHorizontalLayout())
        d->centralLayout()->setDirection(QBoxLayout::LeftToRight);
    else
        d->centralLayout()->setDirection(QBoxLayout::TopToBottom);

    QWidget::resizeEvent(event);
}

} // namespace lmrs::studio

#include "moc_variableeditorview.cpp"
