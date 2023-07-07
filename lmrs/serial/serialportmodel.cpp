#include "serialportmodel.h"

#include <lmrs/core/logging.h>

#include <QSerialPortInfo>

namespace lmrs::serial {

SerialPortModel::SerialPortModel(ProbeFunction probeSerialPort, QObject *parent)
    : ChoiceListModel{QMetaType::fromType<QString>(), parent}
    , m_probeSerialPort{std::move(probeSerialPort)}
{
    startScanning();
}

bool SerialPortModel::isScanning() const
{
    return m_scanningTimerId != -1;
}

void SerialPortModel::startScanning()
{
    if (LMRS_FAILED(core::logger(this), !isScanning()))
        return;

    m_scanningTimerId = startTimer(5s);
    updateSerialPorts();
}

void SerialPortModel::stopScanning()
{
    if (LMRS_FAILED(core::logger(this), isScanning()))
        return;

    killTimer(std::exchange(m_scanningTimerId, -1));
}

void SerialPortModel::timerEvent(QTimerEvent *event)
{
    if (isScanning() && event->timerId() == m_scanningTimerId) {
        updateSerialPorts();
        return;
    }

    ChoiceListModel::timerEvent(event);
}

void SerialPortModel::updateSerialPorts()
{
    auto previousPortNames = values();

    for (const auto &info: QSerialPortInfo::availablePorts()) {
        if (m_probeSerialPort && !m_probeSerialPort(info))
            continue;

        if (previousPortNames.removeOne(info.portName()))
            continue;

        auto description = info.portName();

        if (auto text = info.description(); !text.isEmpty())
            description += " ("_L1 + text + ')'_L1;

        append(std::move(description), info.portName());
    }

    for (const auto &portName: previousPortNames)
        removeAll(portName);
}

} // namespace lmrs::serial
