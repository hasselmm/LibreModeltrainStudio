#include "debugview.h"

#include <lmrs/core/memory.h>
#include <lmrs/core/userliterals.h>

#include <QBoxLayout>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace lmrs::studio {

namespace {

template<class LayoutType, typename... Args>
[[nodiscard]] LayoutType *setupFrame(QFrame *frame, QString text, Args... args)
{
    const auto label = new QLabel{text, frame};
    label->setStyleSheet("font-weight: bold"_L1);

    const auto layout = new LayoutType{frame};

//    if constexpr (std::is_base_of<QFormLayout, LayoutType>::value) {
//        layout->addRow(label);
//    } else {
        layout->addWidget(label, args...);
//    }

    return layout;
}

quint8 checksum(char *first, char *last) // FIXME: move into shared header
{
    auto checksum = quint8{};

    for (auto it = first; it != last; ++it)
        checksum ^= static_cast<quint8>(*it);

    return checksum;
}

} // namespace

class DebugView::Private : public core::PrivateObject<DebugView>
{
public:
    using PrivateObject::PrivateObject;

    void setupDccFrame();
    void setupNativeFrame();

    void onDccSendButtonClicked();
    void onNativeSendButtonClicked();

    QPointer<core::DebugControl> debugControl;

    core::ConstPointer<QFrame> dccFrame{q()};
    core::ConstPointer<QComboBox> dccFrameBox{dccFrame};
    core::ConstPointer<QComboBox> dccFeedbackModeBox{dccFrame};
    core::ConstPointer<QComboBox> dccPowerModeBox{dccFrame};

    core::ConstPointer<QFrame> nativeFrame{q()};
    core::ConstPointer<QComboBox> nativeFrameBox{nativeFrame};
};

void DebugView::Private::setupDccFrame()
{
    dccFrameBox->setEditable(true);

    dccFrameBox->addItem(tr("Reset"),                      "00 00 00"_L1);
    dccFrameBox->addItem(tr("Forward, stop (28S) @830"),   "C3 3E 60 00"_L1);
    dccFrameBox->addItem(tr("Backward, stop (28S) @830"),  "C3 3E 40 00"_L1);
    dccFrameBox->addItem(tr("Emergency stop (28S) @830"),  "C3 3E 60 01"_L1);
    dccFrameBox->addItem(tr("Forward, stop (128S) @830"),  "C3 3E 3F 80 00"_L1);
    dccFrameBox->addItem(tr("Backward, stop (128S) @830"), "C3 3E 3F 00 00"_L1);
    dccFrameBox->addItem(tr("Enable front light @830"),    "C3 3E 90 00"_L1);
    dccFrameBox->addItem(tr("Disable front light @830"),   "C3 3E 80 00"_L1);
    dccFrameBox->addItem(tr("Enable gear light @830"),     "C3 3E DE 02 00"_L1);
    dccFrameBox->addItem(tr("Disable gear light @830"),    "C3 3E DE 00 00"_L1);
    dccFrameBox->addItem(tr("Enable cab light @830"),      "C3 3E DE 01 00"_L1);
    dccFrameBox->addItem(tr("Disable cab light @830"),     "C3 3E DE 00 00"_L1);
    dccFrameBox->addItem(tr("POM Byte Read CV8 @1930"),    "C7 8A E4 08 00 00"_L1);

    const auto dccFrameEdit = dccFrameBox->lineEdit();

    dccFrameEdit->setClearButtonEnabled(true);
    dccFrameEdit->setInputMask(">HH HH HH hh hh hh hh hh hh hh hh hh hh hh hh;_"_L1);
    dccFrameEdit->setText("00 00 00"_L1);

    dccPowerModeBox->addItem(tr("Service mode"), QVariant::fromValue(core::DebugControl::DccPowerMode::Service));
    dccPowerModeBox->addItem(tr("Track mode"), QVariant::fromValue(core::DebugControl::DccPowerMode::Track));

    dccFeedbackModeBox->addItem(tr("Without feedback"), QVariant::fromValue(core::DebugControl::DccFeedbackMode::None));
    dccFeedbackModeBox->addItem(tr("Basic feedback"), QVariant::fromValue(core::DebugControl::DccFeedbackMode::Acknowledge));
    dccFeedbackModeBox->addItem(tr("Advanced feedback"), QVariant::fromValue(core::DebugControl::DccFeedbackMode::Advanced));

    const auto sendButton = new QPushButton{tr("Send &DCC Frame"), dccFrame};

    const auto buttonLayout = new QHBoxLayout;
    buttonLayout->setAlignment(Qt::AlignVCenter);
    buttonLayout->addStretch();
    buttonLayout->addWidget(dccPowerModeBox);
    buttonLayout->addWidget(dccFeedbackModeBox);
    buttonLayout->addWidget(sendButton);

    const auto layout = setupFrame<QVBoxLayout>(dccFrame, tr("DCC Protocol"));

    layout->addWidget(dccFrameBox);
    layout->addLayout(buttonLayout);

    connect(dccFrameBox, &QComboBox::currentIndexChanged, this, [this] {
        dccFrameBox->lineEdit()->setText(dccFrameBox->currentData().toString());
    });

    connect(dccFrameEdit, &QLineEdit::textChanged, this, [sendButton](QString text) {
        sendButton->setEnabled(text.length() >= 2);
    });

    connect(dccFrameEdit, &QLineEdit::returnPressed, sendButton, &QPushButton::click);
    connect(sendButton, &QPushButton::clicked, this, &Private::onDccSendButtonClicked);
}

void DebugView::Private::setupNativeFrame()
{
    nativeFrameBox->setEditable(true);

    const auto nativeFrameEdit = nativeFrameBox->lineEdit();

    nativeFrameEdit->setClearButtonEnabled(true);
    nativeFrameEdit->setInputMask(">HH HH HH hh hh hh hh hh hh hh hh hh hh hh hh;_"_L1);

    const auto sendButton = new QPushButton{tr("Send &Native Frame"), nativeFrame};
    auto title = debugControl ? debugControl->nativeProtocolName() : QString{};

    const auto buttonLayout = new QHBoxLayout;

    buttonLayout->setAlignment(Qt::AlignVCenter);
    buttonLayout->addStretch();
    buttonLayout->addWidget(sendButton);

    const auto layout = setupFrame<QVBoxLayout>(nativeFrame, std::move(title));

    layout->addWidget(nativeFrameBox);
    layout->addLayout(buttonLayout);

    connect(nativeFrameEdit, &QLineEdit::returnPressed, sendButton, &QPushButton::click);
    connect(sendButton, &QPushButton::clicked, this, &Private::onNativeSendButtonClicked);

    connect(nativeFrameBox, &QComboBox::currentIndexChanged, this, [this] {
        const auto frame = nativeFrameBox->currentData().toByteArray();
        nativeFrameBox->lineEdit()->setText(QString::fromLatin1(frame.toHex(' ')));
        qInfo() << frame.toHex(' ');
    });
}

void DebugView::Private::onDccSendButtonClicked()
{
    if (debugControl) {
        auto frame = QByteArray::fromHex(dccFrameBox->currentText().toLatin1());
        const auto powerMode = dccPowerModeBox->currentData().value<core::DebugControl::DccPowerMode>();
        const auto feedbackMode = dccFeedbackModeBox->currentData().value<core::DebugControl::DccFeedbackMode>();

        frame.back() = static_cast<char>(checksum(frame.begin(), frame.end() - 1));
        debugControl->sendDccFrame(std::move(frame), powerMode, feedbackMode);
    }
}

void DebugView::Private::onNativeSendButtonClicked()
{
    if (debugControl) {
        auto text = nativeFrameBox->currentText();
        auto frame = QByteArray::fromHex(text.toLatin1());
        debugControl->sendNativeFrame(std::move(frame));
    }
}

DebugView::DebugView(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    d->setupDccFrame();
    d->setupNativeFrame();

    const auto layout = new QVBoxLayout{this};

    layout->setAlignment(Qt::AlignTop);
    layout->addWidget(d->dccFrame);
    layout->addWidget(d->nativeFrame);
}

void DebugView::setDebugControl(core::DebugControl *newControl)
{
    if (const auto oldControl = std::exchange(d->debugControl, newControl); newControl != oldControl) {
        if (oldControl)
            oldControl->disconnect(d);

        if (newControl) {
            d->nativeFrame->findChild<QLabel *>()->setText(newControl->nativeProtocolName());

            using Feature = core::DebugControl::Feature;

            d->dccFrame->setEnabled(newControl->features() & Feature::DccFrames);
            d->nativeFrame->setEnabled(newControl->features() & Feature::NativeFrames);
            d->nativeFrameBox->clear();

            for (auto [text, sequence]: newControl->nativeExampleFrames())
                d->nativeFrameBox->addItem(std::move(text), std::move(sequence));
        } else {
            d->dccFrame->setEnabled(false);
            d->nativeFrame->setEnabled(false);
        }
    }
}

core::DebugControl *DebugView::debugControl() const
{
    return d->debugControl;
}

} // namespace lmrs::studio
