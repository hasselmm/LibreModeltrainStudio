#include "mainwindow.h"

#include "lokprogrammer.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/dccconstants.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/esu/lp2request.h>
#include <lmrs/esu/lp2response.h>
#include <lmrs/esu/lp2stream.h>

#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QDir>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVersionNumber>

using namespace lmrs; // FIXME

namespace esu::programmer {

namespace {

Q_LOGGING_CATEGORY(lcMainWindow, "top.mainwindow");

using dcc::VehicleVariable;

constexpr auto s_settingLastDebugScript = "Debug/LastScript"_L1;

template<class LayoutType = QFormLayout, typename... Args>
[[nodiscard]] LayoutType *setupFrame(QFrame *frame, QString text, Args... args)
{
    const auto layout = new LayoutType{frame};
    const auto label = new QLabel{text, frame};
    label->setStyleSheet("font-weight: bold"_L1);

    if constexpr (std::is_base_of<QFormLayout, LayoutType>::value) {
        layout->addRow(label);
    } else {
        layout->addWidget(label, args...);
    }

    return layout;
}

} // namespace

class MainWindow::Private : public core::PrivateObject<MainWindow>
{
public:
    using PrivateObject::PrivateObject;

    ConstPointer<LokProgrammer> programmer{this};

    ConstPointer<QFrame> connectFrame{q()};
    ConstPointer<QComboBox> serialPortBox{connectFrame};
    ConstPointer<QPushButton> connectButton{tr("&Connect"), connectFrame};
    ConstPointer<QPushButton> disconnectButton{tr("&Disconnect"), connectFrame};

    ConstPointer<QFrame> debugDccFrame{q()};
    ConstPointer<QLineEdit> debugDccEdit{debugDccFrame};
    ConstPointer<QPushButton> debugDccSendButton{tr("&Send DCC Command"), debugDccFrame};
    ConstPointer<QComboBox> debugDccFeedbackModeBox{debugDccFrame};
    ConstPointer<QCheckBox> debugDccProgrammingModeBox{tr("Pr&ogramming mode"), debugDccFrame};

    ConstPointer<QFrame> debugUsartFrame{q()};
    ConstPointer<QLineEdit> debugUsartEdit{debugUsartFrame};
    ConstPointer<QPushButton> debugUsartSendButton{tr("Send &USART Command"), debugUsartFrame};

    ConstPointer<QFrame> debugParsingFrame{q()};
    ConstPointer<QComboBox> debugRecordingsBox{debugParsingFrame};
    ConstPointer<QPushButton> parseDebugRecordingButton{tr("&Parse Recording"), debugParsingFrame};
    ConstPointer<QCheckBox> debugShowCommentsBox{tr("Show Comments"), debugParsingFrame};

    void setupConnectFrame();
    void onConnectButtonPressed();
    void onDisconnectButtonPressed();

    void setupDebugDccFrame();
    void onDebugDccSendButtonPressed();

    void setupDebugUsartFrame();
    void onDebugUsartSendButtonPressed();

    void setupDebugParsingFrame();
    void onParseRecordingButtonPressed();

    void onProgrammerStateChanged(LokProgrammer::State state);
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}
    , d{new Private{this}}
{
    d->setupConnectFrame();
    d->setupDebugDccFrame();
    d->setupDebugUsartFrame();
    d->setupDebugParsingFrame();

    const auto deviceColumn = new QVBoxLayout;
    deviceColumn->addWidget(d->connectFrame);

    const auto debugColumn = new QVBoxLayout;
    debugColumn->addWidget(d->debugDccFrame);
    debugColumn->addWidget(d->debugUsartFrame);
    debugColumn->addWidget(d->debugParsingFrame);
    debugColumn->addStretch(1);

    const auto centralWidget = new QWidget{this};
    setCentralWidget(centralWidget);

    const auto layout = new QHBoxLayout{centralWidget};
    layout->addLayout(deviceColumn, 1);
    layout->addLayout(debugColumn, 1);

    connect(d->programmer, &LokProgrammer::stateChanged, d, &Private::onProgrammerStateChanged);
    d->onProgrammerStateChanged(d->programmer->state());
}

void MainWindow::Private::setupConnectFrame()
{
    for (const auto &portName: LokProgrammer::availablePorts())
        serialPortBox->addItem(portName, portName);

    const auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(connectButton);
    buttonLayout->addWidget(disconnectButton);

    const auto layout = setupFrame(connectFrame, tr("Connection"));
    layout->addRow(tr("&Serial Port:"), serialPortBox);
    layout->addRow(nullptr, buttonLayout);

    connect(connectButton, &QPushButton::pressed, this, &Private::onConnectButtonPressed);
    connect(disconnectButton, &QPushButton::pressed, this, &Private::onDisconnectButtonPressed);
}

void MainWindow::Private::onConnectButtonPressed()
{
    if (programmer->state() != LokProgrammer::State::Disconnected)
        programmer->disconnectFromDevice();

    programmer->connectToDevice(serialPortBox->currentData().toString());
}

void MainWindow::Private::onDisconnectButtonPressed()
{
    programmer->disconnectFromDevice();
}

void MainWindow::Private::setupDebugParsingFrame()
{
    // fill list of debug scripts
    for (const auto dir = QDir{":/taschenorakel.de/data/traces"_L1}; const auto &fileName: dir.entryList(QDir::Files))
        debugRecordingsBox->addItem(fileName, dir.filePath(fileName));

    // restore last selection
    if (const auto lastScript = debugRecordingsBox->findData(QSettings{}.value(s_settingLastDebugScript)))
        debugRecordingsBox->setCurrentIndex(lastScript);

    const auto layout = setupFrame<QVBoxLayout>(debugParsingFrame, tr("LP2 Parsing"));
    layout->addWidget(debugRecordingsBox);
    layout->addWidget(parseDebugRecordingButton);
    layout->addWidget(debugShowCommentsBox);

    connect(parseDebugRecordingButton, &QPushButton::pressed, this, &Private::onParseRecordingButtonPressed);
}

void MainWindow::Private::onParseRecordingButtonPressed()
{
    auto file = QFile{debugRecordingsBox->currentData().toString()};

    if (!file.open(QFile::ReadOnly)) {
        qCWarning(lcMainWindow, "%ls", qUtf16Printable(file.errorString()));
        return;
    }

    QSettings{}.setValue(s_settingLastDebugScript, file.fileName());
    QHash<Message::Sequence, Request> pendingRequests;
    qCInfo(lcMainWindow) << file.fileName();

    auto outputReader = StreamReader{};
    auto inputReader = StreamReader{};

    while (!file.atEnd()) {
        auto line = file.readLine();

        if (const auto i = line.indexOf('#'); i >= 0) {
            if (debugShowCommentsBox->isChecked())
                qCInfo(lcMainWindow, "%s", line.mid(i + 1).trimmed().constData());

            line.truncate(i);
        }

        line = line.trimmed();

        if (line.startsWith(">W"_L1)) {
            outputReader.addData(QByteArray::fromHex(line.mid(2)));

            while (outputReader.readNext()) {
                if (auto request = Request{outputReader.frame()}; request.isValid()) {
                    qCInfo(lcMainWindow) << ">W" << request.sequence() << request;
                    pendingRequests.insert(request.sequence(), std::move(request));
                } else {
                    qCWarning(lcMainWindow, "bad request");
                }
            }
        } else if (line.startsWith("<R"_L1)) {
            inputReader.addData(QByteArray::fromHex(line.mid(2)));

            while (inputReader.readNext()) {
                auto message = Message{inputReader.frame()};
                auto request = pendingRequests.value(message.sequence());

                if (const auto response = Response{std::move(request), std::move(message)}; response.isValid()) {
                    qCInfo(lcMainWindow) << "<R" << response.sequence() << response;
                } else {
                    qCWarning(lcMainWindow, "bad response");
                }
            }
        } else if (!line.isEmpty()) {
            qCWarning(lcMainWindow, "Unexpected line: %s", line.trimmed().constData());
        }
    }
}

void MainWindow::Private::setupDebugDccFrame()
{
    static constexpr auto createItem = [](QString label, QString command) {
        const auto item = new QStandardItem{std::move(label)};
        item->setData(std::move(command), Qt::UserRole);
        return item;
    };

    const auto completionModel = new QStandardItemModel{debugDccEdit};
    completionModel->appendRow(createItem(tr("Reset"),                      "00 00 00"_L1));
    completionModel->appendRow(createItem(tr("Forward, stop (28S) @830"),   "C3 3E 60 00"_L1));
    completionModel->appendRow(createItem(tr("Backward, stop (28S) @830"),  "C3 3E 40 00"_L1));
    completionModel->appendRow(createItem(tr("Emergency stop (28S) @830"),  "C3 3E 60 01"_L1));
    completionModel->appendRow(createItem(tr("Forward, stop (128S) @830"),  "C3 3E 3F 80 00"_L1));
    completionModel->appendRow(createItem(tr("Backward, stop (128S) @830"), "C3 3E 3F 00 00"_L1));
    completionModel->appendRow(createItem(tr("Enable front light @830"),    "C3 3E 90 00"_L1));
    completionModel->appendRow(createItem(tr("Disable front light @830"),   "C3 3E 80 00"_L1));
    completionModel->appendRow(createItem(tr("Enable gear light @830"),     "C3 3E DE 02 00"_L1));
    completionModel->appendRow(createItem(tr("Disable gear light @830"),    "C3 3E DE 00 00"_L1));
    completionModel->appendRow(createItem(tr("Enable cab light @830"),      "C3 3E DE 01 00"_L1));
    completionModel->appendRow(createItem(tr("Disable cab light @830"),     "C3 3E DE 00 00"_L1));
    completionModel->appendRow(createItem(tr("POM Byte Read CV8 @1930"),    "C7 8A E4 08 00 00"_L1));

    const auto completer = new QCompleter{completionModel, this};
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer->setCompletionRole(Qt::UserRole);
    completer->setMaxVisibleItems(20);

    debugDccEdit->setCompleter(completer);
    debugDccEdit->setClearButtonEnabled(true);
    debugDccEdit->setInputMask(">HH HH HH hh hh hh hh hh hh hh hh hh hh hh hh;_"_L1);
    debugDccEdit->setText("00 00 00"_L1);

    debugDccFeedbackModeBox->addItem(tr("Without feedback"), QVariant::fromValue(DccSettings::nack()));
    debugDccFeedbackModeBox->addItem(tr("Basic feedback"), QVariant::fromValue(DccSettings::ack()));
    debugDccFeedbackModeBox->addItem(tr("Advanced feedback"), QVariant::fromValue(DccSettings::afb()));

    const auto layout = setupFrame<QVBoxLayout>(debugDccFrame, tr("DCC Debugging"));
    layout->addWidget(debugDccEdit);
    layout->addWidget(debugDccSendButton);
    layout->addWidget(debugDccFeedbackModeBox);
    layout->addWidget(debugDccProgrammingModeBox);

    connect(debugDccEdit, &QLineEdit::returnPressed, debugDccSendButton, &QPushButton::click);
    connect(debugDccSendButton, &QPushButton::pressed, this, &Private::onDebugDccSendButtonPressed);
}

void MainWindow::Private::onDebugDccSendButtonPressed()
{
    auto settings = debugDccFeedbackModeBox->currentData().value<DccSettings>();
    qInfo() << "settings:" << settings;
    auto data = QByteArray::fromHex(debugDccEdit->text().toLatin1());

    if (data.size() < 2)
        return;

    auto checksum = quint8{};
    for (auto it = data.begin(); it != data.end() - 1; ++it)
        checksum ^= static_cast<quint8>(*it);
    data.back() = static_cast<char>(checksum);

    const auto dcc = DccRequest{std::move(settings), std::move(data)};
    const auto powerMode = (debugDccProgrammingModeBox->isChecked() ? PowerSettings::Service : PowerSettings::Enabled);

    debugDccFrame->setEnabled(false); // FIXME: do this via Programmer::state()

    programmer->powerOn(PowerSettings{powerMode}, [this, dcc](auto result) {
        if (result != LokProgrammer::Result::Success) {
            debugDccFrame->setEnabled(true); // FIXME: do this via Programmer::state()
            return;
        }

        auto request = Request{Request::Identifier::SendDcc, dcc.toByteArray()};
        programmer->sendRequest(std::move(request), [this](auto response) {
            qInfo() << "debug command response:" << response;

            const auto restoreUI = [this](LokProgrammer::Result) {
                debugDccFrame->setEnabled(true); // FIXME: do this via Programmer::state()
            };

            if (debugDccProgrammingModeBox->isChecked()) {
                programmer->powerOff(restoreUI);
            } else {
                restoreUI(LokProgrammer::Result::Success);
            }
        });
    });
}

void MainWindow::Private::setupDebugUsartFrame()
{
    debugUsartEdit->setClearButtonEnabled(true);
    debugUsartEdit->setInputMask(">HH HH HH hh hh hh hh hh hh hh hh hh hh hh hh;_"_L1);
    debugUsartEdit->setText("00 02 00 00"_L1);

    const auto layout = setupFrame<QVBoxLayout>(debugUsartFrame, tr("USART Debugging"));
    layout->addWidget(debugUsartEdit);
    layout->addWidget(debugUsartSendButton);

    connect(debugUsartEdit, &QLineEdit::returnPressed, debugUsartSendButton, &QPushButton::click);
    connect(debugUsartSendButton, &QPushButton::pressed, this, &Private::onDebugUsartSendButtonPressed);
}

namespace {

static const auto sendSettings = "14 79"_hex;
static const auto readManifacturerId = "00 02 00 02"_hex;
static const auto receiveSettings = "14 79 05"_hex;

using WhateverCallback = std::function<void(QList<Response>)>;

void whatever(LokProgrammer *programmer, QList<Request> requests, QList<Response> responses, WhateverCallback callback)
{
    if (requests.isEmpty()) {
        qInfo() << "finished";
        callback(std::move(responses));
        return;
    }

    auto nextRequest = requests.takeFirst();
    qInfo() << "sending:" << nextRequest.sequence() << nextRequest;
    programmer->sendRequest(std::move(nextRequest),
                            [programmer, requests, responses, callback](Response response) mutable {
        qInfo() << "received:" << response.sequence() << response;

        if (response.identifier() == Response::Identifier::DccResponse && response.dataSize() > 1)
            qInfo().nospace() << "DCC length=" << response.dataSize();

        responses.append(std::move(response));
        whatever(programmer, std::move(requests), std::move(responses), std::move(callback));
    });
}

void whatever(LokProgrammer *programmer, QList<Request> requests, WhateverCallback callback)
{
    whatever(programmer, std::move(requests), {}, std::move(callback));
}

void calibrate(LokProgrammer *programmer, quint8 magic, WhateverCallback callback)
{
    std::array magicVariations = {
        static_cast<quint8>(magic),
        static_cast<quint8>(magic - 2),
        static_cast<quint8>(magic + 2), // -> return magic,         if -2 and +2 succeeded
//        static_cast<quint8>(magic - 4), // -> return magic - 2,     if -2 succeeded
//        static_cast<quint8>(magic + 4), // -> return magic + 2
    };

    QList<Request> requests;

    for (auto p: magicVariations) {
        QList nextRound = {
            Request{Request::Identifier::SendDcc, "64 64 04 02 00 00"_hex},
            Request{Request::Identifier::SetSomeMagic1, "02"_hex},
            Request{Request::Identifier::SetSomeMagic2, QByteArray{reinterpret_cast<const char *>(&p), 1}},
            Request{Request::Identifier::SetSomeMagic1, "00"_hex},
            Request{Request::Identifier::Wait, "05"_hex},
            Request{Request::Identifier::SendUfm, sendSettings + readManifacturerId},
            Request{Request::Identifier::Wait, "01"_hex},
            Request{Request::Identifier::ReceiveUfm, receiveSettings},
        };

        requests += nextRound;
    }

    whatever(programmer, std::move(requests), std::move(callback));
}

}

void MainWindow::Private::onDebugUsartSendButtonPressed()
{
    auto data = QByteArray::fromHex(debugUsartEdit->text().toLatin1());

    if (data.size() < 3)
        return;

    auto checksum = quint8{};
    for (auto it = data.begin(); it != data.end() - 1; ++it)
        checksum ^= static_cast<quint8>(*it);
    data.back() = static_cast<char>(checksum);

    debugUsartFrame->setEnabled(false); // FIXME: do this via Programmer::state()

    programmer->powerOn(PowerSettings{PowerSettings::Service, 32}, [this, data](auto result) {
        if (result != LokProgrammer::Result::Success) {
            debugUsartFrame->setEnabled(true); // FIXME: do this via Programmer::state()
            return;
        }

        QList requests = {
            Request{Request::Identifier::SendDcc, "0c 0c 64 64 00 00 81"_hex},
            Request{Request::Identifier::SetSomeMagic1, "02"_hex},
            Request{Request::Identifier::SendDcc, "0c 0c 64 64 64 00 81"_hex},
            Request{Request::Identifier::SetSomeMagic1, "00"_hex},
            Request{Request::Identifier::Wait, "05"_hex},
            Request{Request::Identifier::SendDcc, "14 14 14 0a 00 00 81"_hex},
            Request{Request::Identifier::SetSomeMagic1, "02 81"_hex},
            Request{Request::Identifier::SendDcc, "14 14 14 0a 04 00 81"_hex},
        };

        whatever(programmer, requests, [this, data](auto) {
            calibrate(programmer, 16, [this, data](auto) {
//                calibrate(programmer, 24, [this] {
//                    calibrate(programmer, 12, [this] {
//                        calibrate(programmer, 30, [this] {
//                            calibrate(programmer, 10, [this] {
//                                calibrate(programmer, 14, [this] {
//                                    calibrate(programmer, 18, [this] {
//                                        calibrate(programmer, 20, [this] {
//                                            calibrate(programmer, 22, [this] {
//                                                calibrate(programmer, 26, [this] {
//                                                    calibrate(programmer, 28, [this] {
//                                                        calibrate(programmer, 32, [this] {
//                                                            calibrate(programmer, 34, [this] {
//                                                                calibrate(programmer, 36, [this] {
//                                                                    calibrate(programmer, 38, [this] {

/*
                <dec:variable index="255" number="257">
                 <dec:item slice="7:0" access="ro" name="info.esu.manid.0"/>    <-- RCN-217 (RailCom) -->
                </dec:variable>
                <dec:variable index="255" number="258">
                 <dec:item slice="7:0" access="ro" name="info.esu.manid.1"/>
                </dec:variable>
                <dec:variable index="255" number="259">
                 <dec:item slice="7:0" access="ro" name="info.esu.manid.2"/>
                </dec:variable>
                <dec:variable index="255" number="260">
                 <dec:item slice="7:0" access="ro" name="info.esu.manid.3"/>
                </dec:variable>
                <dec:variable index="255" number="261">
                 <dec:item slice="7:0" access="ro" name="info.esu.proid.0"/>    <-- RCN-217 (RailCom) -->
                </dec:variable>
                <dec:variable index="255" number="262">
                 <dec:item slice="7:0" access="ro" name="info.esu.proid.1"/>
                </dec:variable>
                <dec:variable index="255" number="263">
                 <dec:item slice="7:0" access="ro" name="info.esu.proid.2"/>
                </dec:variable>
                <dec:variable index="255" number="264">
                 <dec:item slice="7:0" access="ro" name="info.esu.proid.3"/>
                </dec:variable>
                <dec:variable index="255" number="265">
                 <dec:item slice="7:0" access="ro" name="info.esu.serno.0"/>    <-- RCN-217 (RailCom) -->
                </dec:variable>
                <dec:variable index="255" number="266">
                 <dec:item slice="7:0" access="ro" name="info.esu.serno.1"/>
                </dec:variable>
                <dec:variable index="255" number="267">
                 <dec:item slice="7:0" access="ro" name="info.esu.serno.2"/>
                </dec:variable>
                <dec:variable index="255" number="268">
                 <dec:item slice="7:0" access="ro" name="info.esu.serno.3"/>
                </dec:variable>
                <dec:variable index="255" number="269">
                 <dec:item slice="7:0" access="ro" name="info.esu.pdate.0"/>    <-- RCN-217 (RailCom) -->
                </dec:variable>
                <dec:variable index="255" number="270">
                 <dec:item slice="7:0" access="ro" name="info.esu.pdate.1"/>
                </dec:variable>
                <dec:variable index="255" number="271">
                 <dec:item slice="7:0" access="ro" name="info.esu.pdate.2"/>
                </dec:variable>
                <dec:variable index="255" number="272">
                 <dec:item slice="7:0" access="ro" name="info.esu.pdate.3"/>
                </dec:variable>
                <dec:variable index="255" number="273">
                 <dec:item slice="7:0" access="ro" name="info.esu.pinfo.0"/>
                </dec:variable>
                <dec:variable index="255" number="274">
                 <dec:item slice="7:0" access="ro" name="info.esu.pinfo.1"/>
                </dec:variable>
                <dec:variable index="255" number="275">
                 <dec:item slice="7:0" access="ro" name="info.esu.pinfo.2"/>
                </dec:variable>
                <dec:variable index="255" number="276">
                 <dec:item slice="7:0" access="ro" name="info.esu.pinfo.3"/>
                </dec:variable>
                <dec:variable index="255" number="277">
                 <dec:item slice="7:0" access="ro" name="info.esu.bcode.0"/>
                </dec:variable>
                <dec:variable index="255" number="278">
                 <dec:item slice="7:0" access="ro" name="info.esu.bcode.1"/>
                </dec:variable>
                <dec:variable index="255" number="279">
                 <dec:item slice="7:0" access="ro" name="info.esu.bcode.2"/>
                </dec:variable>
                <dec:variable index="255" number="280">
                 <dec:item slice="7:0" access="ro" name="info.esu.bcode.3"/>
                </dec:variable>
                <dec:variable index="255" number="281">
                 <dec:item slice="7:0" access="ro" name="info.esu.bdate.0"/>
                </dec:variable>
                <dec:variable index="255" number="282">
                 <dec:item slice="7:0" access="ro" name="info.esu.bdate.1"/>
                </dec:variable>
                <dec:variable index="255" number="283">
                 <dec:item slice="7:0" access="ro" name="info.esu.bdate.2"/>
                </dec:variable>
                <dec:variable index="255" number="284">
                 <dec:item slice="7:0" access="ro" name="info.esu.bdate.3"/>
                </dec:variable>
                <dec:variable index="255" number="285">
                 <dec:item slice="7:0" access="ro" name="info.esu.acode.0"/>
                </dec:variable>
                <dec:variable index="255" number="286">
                 <dec:item slice="7:0" access="ro" name="info.esu.acode.1"/>
                </dec:variable>
                <dec:variable index="255" number="287">
                 <dec:item slice="7:0" access="ro" name="info.esu.acode.2"/>
                </dec:variable>
                <dec:variable index="255" number="288">
                 <dec:item slice="7:0" access="ro" name="info.esu.acode.3"/>
                </dec:variable>
                <dec:variable index="255" number="289">
                 <dec:item slice="7:0" access="ro" name="info.esu.adate.0"/>
                </dec:variable>
                <dec:variable index="255" number="290">
                 <dec:item slice="7:0" access="ro" name="info.esu.adate.1"/>
                </dec:variable>
                <dec:variable index="255" number="291">
                 <dec:item slice="7:0" access="ro" name="info.esu.adate.2"/>
                </dec:variable>
                <dec:variable index="255" number="292">
                 <dec:item slice="7:0" access="ro" name="info.esu.adate.3"/>
                </dec:variable>
                <dec:variable index="255" number="293">
                 <dec:item slice="7:0" access="ro" name="info.esu.atype.0"/>
                </dec:variable>
                <dec:variable index="255" number="294">
                 <dec:item slice="7:0" access="ro" name="info.esu.atype.1"/>
                </dec:variable>
                <dec:variable index="255" number="295">
                 <dec:item slice="7:0" access="ro" name="info.esu.atype.2"/>
                </dec:variable>
                <dec:variable index="255" number="296">
                 <dec:item slice="7:0" access="ro" name="info.esu.atype.3"/>
                </dec:variable>
*/

                enum class DecoderInfo : quint8 {
                    ManId = 0x00,
                    ProductId = 0x01,
                    SerialNumber = 0x02,
                    ProdDate = 0x03,
                    ProdInfo = 0x04,
                    BootDate = 0x05,
                    BootInfo = 0x06,
                    ApplCode = 0x07,
                    ApplData = 0x08,
                    ApplType = 0x09,

                    First = ManId,
                    Last = ApplType,
                };

                QList<Request> requests;

                for (auto info = static_cast<quint8>(DecoderInfo::First); info <= static_cast<quint8>(DecoderInfo::Last); ++info) {
                    auto data = readManifacturerId;
                    *reinterpret_cast<quint8 *>(data.data() + 2) = info;
                    *reinterpret_cast<quint8 *>(data.data() + 3) ^= info;

                    requests.append({
                                        Request{Request::Identifier::SendUfm, sendSettings + data},
                                        Request{Request::Identifier::Wait, "01"_hex},
                                        Request{Request::Identifier::ReceiveUfm, receiveSettings},
                                    });
                }

                whatever(programmer, requests, [this](QList<Response> responseList) {
                    std::sort(responseList.begin(), responseList.end(), [](const Response &lhs, const Response &rhs) {
                        return std::make_tuple(lhs.type(), lhs.sequence())
                                < std::make_tuple(rhs.type(), rhs.sequence());
                    });

                    Request request;

                    for (const auto &response: responseList) {
                        const auto id = response.request().identifier();

                        if (id == Request::Identifier::SendUfm) {
                            request = response.request();
                        } else if (response.request().identifier() == Request::Identifier::ReceiveUfm) {
                            qInfo() << request.get<UfmSendRequest>()->data().toHex(' ')
                                    << response.get<DccResponse>()->data.toHex(' ');
                        }
                    }

                    programmer->powerOff([this](auto) {
                        debugUsartFrame->setEnabled(true); // FIXME: do this via Programmer::state()
                    });
                });
//                                                                    });
//                                                                });
//                                                            });
//                                                        });
//                                                    });
//                                                });
//                                            });
//                                        });
//                                    });
//                                });
//                            });
//                        });
//                    });
//                });
            });
        });

//        16,
//        24,
//        12,
//        30,
//        10,
//        14,
//        18,
//        20,
//        22,
//        26,
//        28,
//        32,
//        34,
//        36,
//        38

        /*
        qInfo("SendDCC(ESUMode)");
        programmer->sendRequest(, printResponse);
        qInfo("SetMagic1(2)");

        auto request = ;
        qInfo() << request;
        programmer->sendRequest(std::move(request), [this](auto response) {
            qInfo() << "UartSend response:" << response;

            auto pause = Request;
            qInfo() << pause;

            programmer->sendRequest(std::move(pause), [this](auto response) {
                qInfo() << "pause response:" << response;

                auto request = ;
                qInfo() << request;

                programmer->sendRequest(std::move(request), [this](auto response) {
                    qInfo() << "UartReceive response:" << response;

                    programmer->powerOff([this](LokProgrammer::Result) {
                        debugUsartFrame->setEnabled(true); // FIXME: do this via Programmer::state()
                    });
                });
            });
        });
        */
    });
}

void MainWindow::Private::onProgrammerStateChanged(LokProgrammer::State state)
{
    connectButton->setEnabled(state == LokProgrammer::State::Disconnected);
    disconnectButton->setEnabled(state != LokProgrammer::State::Disconnected);
    debugDccFrame->setEnabled(state == LokProgrammer::State::Connected);
    debugUsartFrame->setEnabled(state == LokProgrammer::State::Connected);
}

} // namespace esu::programmer
