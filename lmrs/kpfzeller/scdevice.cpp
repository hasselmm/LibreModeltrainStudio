#include "scdevice.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/serial/serialportmodel.h>

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimerEvent>

namespace lmrs::kpfzeller {

namespace {

constexpr auto s_parameterPortName = "port"_L1;
constexpr auto s_parameterScale = "scale"_L1;
constexpr auto s_parameterRubber = "has-rubber"_L1;

constexpr auto s_expectedVendorId = 0x1a86;     // wch.cn
constexpr auto s_expectedProductId = 0x7523;    // CH340

constexpr auto s_vendorName = "KPF-Zeller"_L1;
constexpr auto s_productName = "Speed-Cat Plus"_L1;

bool probeSerialPort(const QSerialPortInfo &info)
{
    return info.vendorIdentifier() == s_expectedVendorId
            && info.productIdentifier() == s_expectedProductId;
}

auto displayName(SpeedCatDevice::Scale scale)
{
    switch (scale) {
    case SpeedCatDevice::Scale::RawPulses:
        return DeviceFactory::tr("Raw Pulses");
    case SpeedCatDevice::Scale::Gauge1:
        return DeviceFactory::tr("1 gauge (1:32)");
    case SpeedCatDevice::Scale::Scale0:
        return DeviceFactory::tr("O scale (1:48)");
    case SpeedCatDevice::Scale::ScaleH0:
        return DeviceFactory::tr("HO scale (1:87)");
    case SpeedCatDevice::Scale::ScaleTT:
        return DeviceFactory::tr("TT scale (1:120)");
    case SpeedCatDevice::Scale::ScaleN:
        return DeviceFactory::tr("N scale (1:160)");
    case SpeedCatDevice::Scale::Invalid:
        break;
    }

    return QString{};
}

auto rubberOffset(SpeedCatDevice::RubberType rubberType)
{
    switch (rubberType) {
    case SpeedCatDevice::RubberType::Red:
        return 0.9;

    case SpeedCatDevice::RubberType::None:
        break;
    }

    return 0.0;
}

auto rollerDiameter(SpeedCatDevice::Scale scale, SpeedCatDevice::RubberType rubberType)
{
    switch (scale) {
    case SpeedCatDevice::Scale::Gauge1:
    case SpeedCatDevice::Scale::Scale0:
    case SpeedCatDevice::Scale::ScaleH0:
        return 5.95 + rubberOffset(rubberType);

    case SpeedCatDevice::Scale::ScaleTT:
    case SpeedCatDevice::Scale::ScaleN:
        return 3.75 + rubberOffset(rubberType);

    case SpeedCatDevice::Scale::RawPulses:
    case SpeedCatDevice::Scale::Invalid:
        break;
    }

    return 0.0;
}

auto pulsesPerTurn(SpeedCatDevice::Scale scale)
{
    switch (scale) {
    case SpeedCatDevice::Scale::Gauge1:
    case SpeedCatDevice::Scale::Scale0:
    case SpeedCatDevice::Scale::ScaleH0:
    case SpeedCatDevice::Scale::ScaleTT:
    case SpeedCatDevice::Scale::ScaleN:
        return 8;

    case SpeedCatDevice::Scale::RawPulses:
        return 1;

    case SpeedCatDevice::Scale::Invalid:
        break;
    }

    return 0;
}

auto ratio(SpeedCatDevice::Scale scale)
{
    switch (scale) {
    case SpeedCatDevice::Scale::Gauge1:
        return 32;
    case SpeedCatDevice::Scale::Scale0:
        return 48;
    case SpeedCatDevice::Scale::ScaleH0:
        return 87;
    case SpeedCatDevice::Scale::ScaleTT:
        return 120;
    case SpeedCatDevice::Scale::ScaleN:
        return 160;

    case SpeedCatDevice::Scale::RawPulses:
        return 1;

    case SpeedCatDevice::Scale::Invalid:
        break;
    }

    return 0;
}

core::millimeters_per_second rawSpeed(core::hertz_f pulses, SpeedCatDevice::Scale scale, SpeedCatDevice::RubberType rubberType)
{
    if (Q_UNLIKELY(scale == SpeedCatDevice::Scale::RawPulses))
        return {};

    // FIXME: [frequency] * [distance] = [speed]
    return qRound(pulses.count() * M_PI * rollerDiameter(scale, rubberType) * ratio(scale) / pulsesPerTurn(scale));
}

auto defaultScales()
{
    auto scales = QList<core::parameters::Choice>{};

    for (const auto &entry: QMetaTypeId<SpeedCatDevice::Scale>())
        if (entry.value() != SpeedCatDevice::Scale::Invalid)
            scales.emplaceBack(displayName(entry.value()), QVariant::fromValue(entry.value()));

    return scales;
}

} // namespace

class SpeedCatDevice::SpeedMeter : public core::SpeedMeterControl
{
public:
    explicit SpeedMeter(Scale scale, RubberType rubber, SpeedCatDevice *parent)
        : core::SpeedMeterControl(parent)
        , scale{scale}
        , rubberType{rubber}
    {}

    Device *device() const override { return core::checked_cast<Device *>(parent()); }

    Features features() const override;

    core::millimeters_per_second filteredSpeed() const override;
    core::millimeters_per_second rawSpeed() const override;
    core::hertz_f pulses() const override;

    void addMeasurement(unsigned pulseCount);

    const Scale scale;
    const RubberType rubberType;

    unsigned sampleCount = 0U;
    size_t currentSample = 0U;

    struct Sample
    {
        using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;

        Timestamp timestamp;
        unsigned pulseCount;
    };

    std::array<Sample, 50> samples;
};

void SpeedCatDevice::SpeedMeter::addMeasurement(unsigned pulseCount)
{
    const auto deviceStateGuard = core::propertyGuard(device(), &core::Device::state,
                                                      &core::Device::stateChanged,
                                                      Device::QProtectedSignal{});
    const auto filteredSpeedGuard = core::propertyGuard(this, &SpeedMeter::filteredSpeed,
                                                        &SpeedMeter::filteredSpeedChanged,
                                                        SpeedMeterControl::QProtectedSignal{});
    const auto rawSpeedGuard = core::propertyGuard(this, &SpeedMeter::rawSpeed,
                                                   &SpeedMeter::rawSpeedChanged,
                                                   SpeedMeterControl::QProtectedSignal{});
    const auto pulseCountGuard = core::propertyGuard(this, &SpeedMeter::pulses,
                                                     &SpeedMeter::pulsesChanged,
                                                     SpeedMeterControl::QProtectedSignal{});

    if (sampleCount > 0)
        currentSample = (currentSample + 1) % samples.size();
    if (sampleCount < samples.size())
        ++sampleCount;

    auto now = Sample::Timestamp::clock::now();
    samples[currentSample] = {now, pulseCount};
    emit dataReceived(std::move(now), SpeedMeterControl::QProtectedSignal{});
}

core::SpeedMeterControl::Features SpeedCatDevice::SpeedMeter::features() const
{
    if (rollerDiameter(scale, rubberType) > 0)
        return Feature::MeasurePulses | Feature::MeasureSpeed;

    return Feature::MeasurePulses;
}

core::millimeters_per_second SpeedCatDevice::SpeedMeter::filteredSpeed() const
{
    const auto latestValueBias = 0U;
    const auto windowSize = qMin(sampleCount, 5U);
    const auto first = currentSample + samples.size() - windowSize;
    const auto last = first + windowSize;

    auto pulseSum = latestValueBias * pulses().count();

    for (auto i = first; i < last; ++i)
        pulseSum += samples[i % samples.size()].pulseCount;

    return kpfzeller::rawSpeed(pulseSum / (windowSize + latestValueBias), scale, rubberType);
}

core::millimeters_per_second SpeedCatDevice::SpeedMeter::rawSpeed() const
{
    return kpfzeller::rawSpeed(pulses(), scale, rubberType);
}

core::hertz_f SpeedCatDevice::SpeedMeter::pulses() const
{
    if (sampleCount == 0)
        return 0;

    return samples[currentSample].pulseCount;
}

class SpeedCatDevice::Private : public core::PrivateObject<SpeedCatDevice>
{
public:
    explicit Private(QString portName, Scale scale, RubberType rubberType,
                     SpeedCatDevice *parent, DeviceFactory *factory)
        : PrivateObject{parent}
        , serialPort{std::move(portName), this}
        , speedMeter{scale, rubberType, q()}
        , factory{factory}
    {}

    void reportError(QString message);
    void onReadyRead();

    void timerEvent(QTimerEvent *event) override;
    void cancelConnectTimeout();

    int connectTimeoutId = 0;

    core::ConstPointer<QSerialPort> serialPort;
    core::ConstPointer<SpeedMeter> speedMeter;
    core::ConstPointer<DeviceFactory> factory;
};

void SpeedCatDevice::Private::reportError(QString message)
{
    qCWarning(logger(), "Error occured: %ls", qUtf16Printable(message));
    q()->disconnectFromDevice();
}

void SpeedCatDevice::Private::onReadyRead()
{
    while (serialPort->canReadLine()) {
        const auto line = serialPort->readLine().trimmed();

        if (line.startsWith('*') && line.endsWith(";V3.0%")) {
            cancelConnectTimeout();

            auto isNumber = false;
            const auto colon = line.indexOf(';', 1);
            const auto end = colon >= 0 ? colon : line.length();

            auto begin = 1;

            while (line[begin] == '0' && begin < end - 1)
                ++begin;

            const auto pulseCount = line.mid(begin, end - begin).toUInt(&isNumber);

            if (isNumber)
                speedMeter->addMeasurement(pulseCount);
        }
    }
}

void SpeedCatDevice::Private::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == connectTimeoutId) {
        cancelConnectTimeout();
        reportError(tr("Timeout reached while trying to connect"));
    }
}

void SpeedCatDevice::Private::cancelConnectTimeout()
{
    if (const auto timerId = std::exchange(connectTimeoutId, 0))
        killTimer(timerId);
}

// =====================================================================================================================

SpeedCatDevice::SpeedCatDevice(QString portName, Scale scale, RubberType rubberType, QObject *parent, DeviceFactory *factory)
    : Device{parent}
    , d{new Private{std::move(portName), scale, rubberType, this, factory}}
{
    connect(d->serialPort, &QSerialPort::readyRead, d, &Private::onReadyRead);
}

core::Device::State SpeedCatDevice::state() const
{
    if (!d->serialPort->isOpen())
        return State::Disconnected;
    if (d->speedMeter->sampleCount == 0)
        return State::Connecting;

    return State::Connected;
}

QString SpeedCatDevice::name() const
{
    if (d->speedMeter->scale == SpeedCatDevice::Scale::RawPulses)
        return tr("%1 at %2 in raw mode").arg(s_productName, d->serialPort->portName());

    return tr("%1 for %2 at %3").arg(s_productName, displayName(d->speedMeter->scale), d->serialPort->portName());
}

QString SpeedCatDevice::uniqueId() const
{
    return "kpfzeller:speedcat:"_L1 + d->serialPort->portName();
}

bool SpeedCatDevice::connectToDevice()
{
    const auto guard = core::propertyGuard(this, &Device::state, &Device::stateChanged, Device::QProtectedSignal{});

    if (!d->serialPort->setBaudRate(QSerialPort::Baud9600)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setStopBits(QSerialPort::OneStop)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setParity(QSerialPort::NoParity)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setDataBits(QSerialPort::Data8)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setFlowControl(QSerialPort::NoFlowControl)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->open(QSerialPort::ReadWrite)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    d->connectTimeoutId = d->startTimer(5s);

    return true;
}

void SpeedCatDevice::disconnectFromDevice()
{
    const auto guard = core::propertyGuard(this, &Device::state, &Device::stateChanged, Device::QProtectedSignal{});

    d->speedMeter->sampleCount = 0;
    d->serialPort->close();
}

core::DeviceFactory *SpeedCatDevice::factory() { return d->factory; }
core::SpeedMeterControl *SpeedCatDevice::speedMeterControl() { return d->speedMeter; }
const core::SpeedMeterControl *SpeedCatDevice::speedMeterControl() const { return d->speedMeter; }

QVariant SpeedCatDevice::deviceInfo(core::DeviceInfo id, int role) const
{
    if (id == core::DeviceInfo::ManufacturerId) {
        if (role == Qt::DisplayRole)
            return s_vendorName;
    } else if (id == core::DeviceInfo::ProductId) {
        if (role == Qt::DisplayRole)
            return s_productName;
    }

    return {};
}

void SpeedCatDevice::updateDeviceInfo()
{
    // nothing to do
}

void SpeedCatDevice::setDeviceInfo(core::DeviceInfo /*id*/, QVariant /*value*/)
{
    // nothing to do
}

// =====================================================================================================================

QString DeviceFactory::name() const
{
    return s_vendorName + ' '_L1 + s_productName;
}

QList<core::Parameter> DeviceFactory::parameters() const
{
    return {
        core::Parameter::choice(s_parameterPortName, LMRS_TR("Serial &port:"),
                                std::make_shared<serial::SerialPortModel>(probeSerialPort)),
        core::Parameter::choice<SpeedCatDevice::Scale>(s_parameterScale, LMRS_TR("Model &scale:"), defaultScales()),
        core::Parameter::flag(s_parameterRubber, LMRS_TR("&Rubber band:"), true),
    };
}

core::Device *DeviceFactory::create(QVariantMap parameters, QObject *parent)
{
    const auto portNameParameter = parameters.find(s_parameterPortName);
    const auto scaleParameter = parameters.find(s_parameterScale);
    const auto rubberParameter = parameters.find(s_parameterRubber);

    if (portNameParameter == parameters.end()) {
        qCWarning(logger(this), "Parameter \"%s\" is missing", s_parameterPortName.data());
        return {};
    } else if (scaleParameter == parameters.end()) {
        qCWarning(logger(this), "Parameter \"%s\" is missing", s_parameterScale.data());
        return {};
    } else if (rubberParameter == parameters.end()) {
        qCWarning(logger(this), "Parameter \"%s\" is missing", s_parameterRubber.data());
        return {};
    }

    if (auto portName = portNameParameter->toString(); portName.isEmpty()) {
        qCWarning(logger(this), "Parameter \"%s\" has unsupported value of type %s",
                  s_parameterPortName.data(), portNameParameter->typeName());
    } else if (const auto scale = scaleParameter->value<SpeedCatDevice::Scale>(); scale == SpeedCatDevice::Scale::Invalid) {
        qCWarning(logger(this), "Parameter \"%s\" has unsupported value of type %s",
                  s_parameterScale.data(), scaleParameter->typeName());
    } else if (const auto hasRubber = rubberParameter->toBool(); rubberParameter->typeId() != QMetaType::Bool) {
        qCWarning(logger(this), "Parameter \"%s\" has unsupported value of type %s",
                  s_parameterRubber.data(), rubberParameter->typeName());
    } else {
        const auto rubber = (hasRubber ? SpeedCatDevice::RubberType::Red : SpeedCatDevice::RubberType::None);
        return new SpeedCatDevice{std::move(portName), scale, rubber, parent, this};
    }

    return nullptr;
}

} // namespace lmrs::kpfzeller
