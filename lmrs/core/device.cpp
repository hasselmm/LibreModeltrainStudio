#include "decoderinfo.h"

#include "accessories.h"
#include "algorithms.h"
#include "detectors.h"
#include "device.h"
#include "quantities.h"
#include "userliterals.h"
#include "vehicleinfomodel.h"

#include <QDateTime>
#include <QLocale>
#include <QPointer>
#include <QVersionNumber>

namespace lmrs::core {

namespace {

QList<QPointer<DeviceFactory>> s_deviceFactories;

template<size_t N>
bool hasPrefix(const char *str, const char (& prefix)[N])
{
    return qstrncmp(str, prefix, N - 1) == 0;
}

QString displayValue(DeviceInfo id, QVariant info)
{
    if (id == DeviceInfo::ProductId)
        return hexString(std::move(info));
    if (info.typeId() == qMetaTypeId<QDateTime>())
        return QLocale{}.toString(info.toDateTime(), QLocale::ShortFormat);
    if (info.typeId() == qMetaTypeId<QVersionNumber>())
        return info.value<QVersionNumber>().toString();
    if (id == DeviceInfo::DevicePort && info.toUInt() == 0)
        return {};

    // FIXME: do this unit handling more globally
    if (info.typeId() == qMetaTypeId<celsius>())
        return toString(info.value<celsius>());
    if (info.typeId() == qMetaTypeId<milliamperes>())
        return toString(info.value<milliamperes>());

    if (info.typeId() == qMetaTypeId<millivolts>()) {
        if (const auto value = info.value<millivolts>(); value >= 500_mV) // FIXME: can we generalize this magic?
            return toString(quantityCast<volts>(value));
        else
            return toString(value);
    }

    if (info.typeId() == qMetaTypeId<volts>())
        return toString(info.value<volts>());

    if (const auto type = info.metaType();
            type.flags() & QMetaType::IsEnumeration
            && hasPrefix(type.name(), "QFlags<")) {
        const auto typeName = QByteArrayView{type.name()};
        const auto valueTypeName = QByteArrayView{typeName.begin() + 7, typeName.end() - 1};

        if (const auto metaEnum = metaEnumFromMetaType(QMetaType::fromName(valueTypeName)); metaEnum.isValid())
            return QString::fromLatin1(metaEnum.valueToKeys(info.toInt()).replace("|", ", "));

        return QString::fromLatin1(typeName) + "(0x"_L1 + QString::number(info.toInt(), 16) + ')'_L1;
    }

    return info.toString();
}

QString displayText(DeviceInfo id, QVariant info, QString text)
{
    if (text.isEmpty() && id == DeviceInfo::ManufacturerId)
        text = dcc::DecoderInfo::vendorName(info.value<dcc::VariableValue::value_type>());
    if (text.isEmpty())
        return displayValue(id, std::move(info));
    if (auto rawValueText = displayValue(id, std::move(info)); !rawValueText.isEmpty())
        text += " ("_L1 + std::move(rawValueText) + ')'_L1;

    return text;
}

} // namespace

// =====================================================================================================================

void VariableControl::readExtendedVariable(dcc::VehicleAddress address, dcc::ExtendedVariableIndex variable,
                                           ContinuationCallback<VariableValueResult> callback)
{
    const auto basicVariable = dcc::variableIndex(variable);

    if (dcc::range(dcc::VariableSpace::Extended).contains(basicVariable)) {
        selectPage(address, dcc::extendedPage(variable), [this, address, basicVariable, callback](Error error) {
            if (error != Error::NoError)
                return Continuation::Retry;

            readVariable(address, basicVariable, std::move(callback));
            return Continuation::Proceed;
        });
    } else if (dcc::range(dcc::VariableSpace::Susi).contains(basicVariable)) {
        selectPage(address, dcc::susiPage(variable), [this, address, basicVariable, callback](Error error) {
            if (error != Error::NoError)
                return Continuation::Retry;

            readVariable(address, basicVariable, std::move(callback));
            return Continuation::Proceed;
        });
    } else {
        readVariable(address, basicVariable, std::move(callback));
    }
}

void VariableControl::readExtendedVariables(dcc::VehicleAddress address, ExtendedVariableList variableList,
                                            ContinuationCallback<dcc::ExtendedVariableIndex, VariableValueResult> callback)
{
    if (variableList.isEmpty())
        return;

    // FIXME: sort reads to minimize CV31/CV32, CV1021 writes; or this is too expensive here?
    const auto variable = variableList.takeFirst();

    readExtendedVariable(address, variable, [=, this](auto result) {
        switch (core::callIfDefined(Continuation::Proceed, callback, variable, result)) {
        case Continuation::Proceed:
            if (!variableList.isEmpty())
                readExtendedVariables(address, variableList, callback);

            break;

        case Continuation::Retry:
            return Continuation::Retry;

        case Continuation::Abort:
            break;
        }

        return Continuation::Proceed;
    });
}

void VariableControl::readExtendedVariables(dcc::VehicleAddress address, ExtendedVariableList variableList,
                                            std::function<void(ExtendedVariableResults)> callback)
{
    if (variableList.isEmpty()) {
        callIfDefined(callback, ExtendedVariableResults{});
        return;
    }

    struct Closure
    {
        qsizetype count;
        decltype(callback) callback;
        std::shared_ptr<ExtendedVariableResults> results = std::make_shared<ExtendedVariableResults>();

        auto operator()(dcc::ExtendedVariableIndex variable, VariableValueResult result)
        {
            results->insert(variable, std::move(result));

            if (results->size() == count)
                core::callIfDefined(callback, *results);

            return Continuation::Proceed;
        }
    };

    auto closure = Closure{variableList.count(), std::move(callback)};
    readExtendedVariables(address, std::move(variableList), std::move(closure));
}

void VariableControl::writeExtendedVariable(dcc::VehicleAddress address,
                                            dcc::ExtendedVariableIndex variable, dcc::VariableValue value,
                                            ContinuationCallback<VariableValueResult> callback)
{
    const auto basicVariable = dcc::variableIndex(variable);

    if (dcc::range(dcc::VariableSpace::Extended).contains(basicVariable)) {
        selectPage(address, dcc::extendedPage(variable), [this, address, basicVariable, value, callback](Error error) {
            if (error != Error::NoError)
                return Continuation::Retry;

            writeVariable(address, basicVariable, value, std::move(callback));
            return Continuation::Proceed;
        });
    } else if (dcc::range(dcc::VariableSpace::Susi).contains(basicVariable)) {
        selectPage(address, dcc::susiPage(variable), [this, address, basicVariable, value, callback](Error error) {
            if (error != Error::NoError)
                return Continuation::Retry;

            writeVariable(address, basicVariable, value, std::move(callback));
            return Continuation::Proceed;
        });
    } else {
        writeVariable(address, basicVariable, value, std::move(callback));
    }
}

void VariableControl::selectPage(dcc::VehicleAddress address, dcc::ExtendedPageIndex page,
                                 ContinuationCallback<Error> callback)
{
    qInfo().verbosity(QDebug::MinimumVerbosity) << Q_FUNC_INFO << __LINE__ << address << page;
    writeVariable(address, variableIndex(dcc::VehicleVariable::ExtendedPageIndexHigh),
                  dcc::cv31(page), [this, address, page, callback](VariableValueResult result) {
        qInfo().verbosity(QDebug::MinimumVerbosity) << Q_FUNC_INFO << __LINE__ << result;
        const auto continuation = core::callIfDefined(retryOnError(result.error), callback, result.error);

        if (continuation == Continuation::Proceed) {
            writeVariable(address, variableIndex(dcc::VehicleVariable::ExtendedPageIndexLow),
                          dcc::cv32(page), [callback](VariableValueResult result) {
                qInfo().verbosity(QDebug::MinimumVerbosity) << Q_FUNC_INFO << __LINE__ << result;
                return core::callIfDefined(retryOnError(result.error), callback, result.error);
            });
        }

        return continuation;
    });
}

void VariableControl::selectPage(dcc::VehicleAddress address, dcc::SusiPageIndex page,
                                 ContinuationCallback<Error> callback)
{
    writeVariable(address, variableIndex(dcc::VehicleVariable::SusiBankIndex),
                  page.value, [callback](VariableValueResult result) {
        return core::callIfDefined(retryOnError(result.error), callback, result.error);
    });
}

// =====================================================================================================================

QString Device::deviceInfoText(DeviceInfo id) const
{
    return deviceInfo(id, Qt::DisplayRole).toString();
}

QString Device::deviceInfoDisplayText(DeviceInfo id) const
{
    return displayText(id, deviceInfo(id), deviceInfoText(id));
}

// =====================================================================================================================

QString DeviceFactory::uniqueId(QVariantMap parameters)
{
    if (const auto device = std::unique_ptr<Device>{create(std::move(parameters), this)})
        return device->uniqueId();

    return {};
}

void DeviceFactory::addDeviceFactory(DeviceFactory *factory)
{
    if (!s_deviceFactories.contains(factory))
        s_deviceFactories.append(factory);
}

QList<DeviceFactory *> DeviceFactory::deviceFactories()
{
    QList<DeviceFactory *> validFactories;

    static const auto isValid = [](auto p) { return !p.isNull(); };
    std::copy_if(s_deviceFactories.begin(), s_deviceFactories.end(),
                 std::back_inserter(validFactories), isValid);

    return validFactories;
}

// =====================================================================================================================

void DeviceInfoModel::setDevice(Device *device)
{
    if (const auto oldDevice = std::exchange(m_device, device); oldDevice != m_device) {
        if (oldDevice)
            oldDevice->disconnect(this);

        beginResetModel();
        m_rows.clear();

        if (m_device) {
            for (const auto &id: QMetaTypeId<DeviceInfo>()) {
                auto value = m_device->deviceInfo(id.value());
                auto text = m_device->deviceInfoText(id.value());

                if (!value.isNull() || !text.isEmpty())
                    m_rows.insert(id.value(), {std::move(value), std::move(text)});
            }

            connect(m_device, &Device::deviceInfoChanged, this, &DeviceInfoModel::onDeviceInfoChanged);
        }

        endResetModel();
    }
}

Device *DeviceInfoModel::device() const
{
    return m_device;
}

int DeviceInfoModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_rows.size());
}

int DeviceInfoModel::columnCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return keyCount<Column>();
}

QVariant DeviceInfoModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        static const auto deviceInfo = QMetaEnum::fromType<DeviceInfo>();
        const auto row = std::next(m_rows.begin(), index.row());

        switch (static_cast<Column>(index.column())) {
        case Column::Name:
            if (role == Qt::DisplayRole) {
                if (const auto key = deviceInfo.key(value(row.key())))
                    return QString::fromLatin1(key);
            }

            break;

        case Column::Value:
            if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
                return displayText(row.key(), row->value, row->text);
            else if (role == Qt::EditRole)
                return row->value;
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

            break;
        }
    }

    return {};
}

QVariant DeviceInfoModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<Column>(section)) {
        case Column::Name:
            return tr("Information");

        case Column::Value:
            return tr("Value");
        }
    }

    return {};
}

void DeviceInfoModel::onDeviceInfoChanged(QList<DeviceInfo> changedIds)
{
    for (auto id: changedIds) {
        auto value = m_device->deviceInfo(id);
        auto text = m_device->deviceInfoText(id);

        const auto it = m_rows.lowerBound(id);
        const auto row = static_cast<int>(std::distance(m_rows.begin(), it));

        if (it == m_rows.end() || it.key() != id) {
            beginInsertRows({}, row, row);
            m_rows.insert(id, {std::move(value), std::move(text)});
            endInsertRows();
        } else {
            it->value = std::move(value);
            it->text = std::move(text);

            static const QList<int> roles = {Qt::EditRole, Qt::DisplayRole};
            const auto rowIndex = index(row, core::value(Column::Value));
            emit dataChanged(rowIndex, rowIndex, roles);
        }
    }
}

Continuation retryOnError(Error error)
{
    if (error != Error::NoError)
        return Continuation::Retry;

    return Continuation::Proceed;
}

void DebugControl::sendDccFrame(QByteArray, DccPowerMode, DccFeedbackMode) const
{
    Q_UNIMPLEMENTED();
}

void DebugControl::sendNativeFrame(QByteArray) const
{
    Q_UNIMPLEMENTED();
}

QString DebugControl::nativeProtocolName() const noexcept
{
    return tr("Native Protocol");
}

QList<QPair<QString, QByteArray>> DebugControl::nativeExampleFrames() const noexcept
{
    return {};
}

} // namespace lmrs::core

#include "moc_device.cpp"
