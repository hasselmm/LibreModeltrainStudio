#ifndef LMRS_SERIAL_SERIALPORTMODEL_H
#define LMRS_SERIAL_SERIALPORTMODEL_H

#include <lmrs/core/parameters.h>

class QSerialPortInfo;

namespace lmrs::serial {

class SerialPortModel : public lmrs::core::parameters::ChoiceListModel
{
    Q_OBJECT

public:
    using ProbeFunction = std::function<bool (const QSerialPortInfo &)>;

    explicit SerialPortModel(ProbeFunction probeSerialPort = {}, QObject *parent = nullptr);

    [[nodiscard]] bool isScanning() const;

public slots:
    void startScanning();
    void stopScanning();

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void updateSerialPorts();

    ProbeFunction m_probeSerialPort;
    int m_scanningTimerId = -1;
};

} // namespace lmrs::serial

#endif // LMRS_SERIAL_SERIALPORTMODEL_H
