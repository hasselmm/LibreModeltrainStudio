#include "automationmodel.h"

#include "logging.h"
#include "parameters.h"
#include "propertyguard.h"
#include "userliterals.h"

#include <QAbstractProxyModel>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaProperty>

namespace lmrs::core::automation {

namespace {

using namespace accessory;

const auto s_schemaBaseUrl = "https://taschenorakel.de/"_url;

constexpr auto s_actions = "$actions"_L1;
constexpr auto s_events = "$events"_L1;
constexpr auto s_schema = "$schema"_L1;

QUrl typeUri(const QMetaObject *metaObject)
{
    auto path = QString::fromLatin1(metaObject->className()).replace("::"_L1, "/"_L1);
    return s_schemaBaseUrl.resolved(QUrl{std::move(path)});
}

template<class T>
auto typeUri()
{
    static const auto uri = typeUri(&T::staticMetaObject);
    return uri;
}

QMetaType metaTypeFromClassName(QByteArrayView className)
{
    auto typeName = QByteArray{};
    typeName.reserve(className.size() + 1);
    typeName.append(std::move(className)).append('*');
    return QMetaType::fromName(std::move(typeName));
}

QMetaType metaTypeFromUri(QUrl uri)
{
    if (!s_schemaBaseUrl.isParentOf(uri))
        return {};

    static const auto prefixLength = s_schemaBaseUrl.path().length();
    auto className = uri.path().mid(prefixLength).replace("/"_L1, "::"_L1).toLatin1();
    return metaTypeFromClassName(std::move(className));
}

class JsonFileReader : public AutomationModelReader
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    using AutomationModelReader::AutomationModelReader;
    ModelPointer read(const AutomationTypeModel *types) override;
};

class JsonFileWriter : public AutomationModelWriter
{
public:
    using AutomationModelWriter::AutomationModelWriter;
    bool write(const AutomationModel *model) override;
};

} // namespace

// =====================================================================================================================

QJsonObject Item::toJsonObject() const
{
    auto object = QJsonObject{{s_schema, typeUri(metaObject()).toString()}};

    for (const auto &p: parameters()) {
        if (metaObject()->indexOfProperty(p.hasValueKey()) >= 0
                && property(p.hasValueKey()).toBool() == false) {
            object.insert(QString::fromLatin1(p.valueKey()), QJsonValue::Null);
        } else {
            // FIXME always verify round-trip conversion?
            auto value = p.toJson(property(p.valueKey()));
            object.insert(QString::fromLatin1(p.valueKey()), std::move(value));
        }
    }

    return object;
}

QByteArray Item::toJson() const
{
    return QJsonDocument{toJsonObject()}.toJson();
}

std::optional<Parameter> Item::parameter(QByteArrayView key) const
{
    for (const auto parameterList = parameters(); const auto &parameter: parameterList) {
        if (parameter.key() == key)
            return parameter;
    }

    return {};
}

// =====================================================================================================================

QJsonObject Event::toJsonObject() const
{
    auto object = Item::toJsonObject();
    auto array = QJsonArray{};

    std::transform(m_actions.begin(), m_actions.end(), std::back_inserter(array),
                   [](const Action *action) { return action->toJsonObject(); });

    object.insert(s_actions, std::move(array));

    return object;
}

int Event::appendAction(Action *action)
{
    return insertAction(action, actionCount());
}

int Event::insertAction(Action *action, int before)
{
    if (LMRS_FAILED_COMPARE(logger(this), before, >=, 0)
            | LMRS_FAILED_COMPARE(logger(this), before, <=, actionCount()))
        return -1;

    action->setParent(this);
    m_actions.insert(before, action);

    emit actionInserted(action, before, {});
    emit actionsChanged({});

    return before;
}

Action *Event::removeAction(int index)
{
    if (LMRS_FAILED_COMPARE(logger(this), index, >=, 0)
            | LMRS_FAILED_COMPARE(logger(this), index, <, actionCount()))
        return nullptr;

    const auto action = m_actions[index];

    if (action && action->parent() == this)
        action->setParent(nullptr);

    m_actions.removeAt(index);

    emit actionRemoved(action, index, {});
    emit actionsChanged({});

    return action;
}

QList<Action *> Event::actions() const
{
    return m_actions;
}

int Event::actionCount() const
{
    return static_cast<int>(m_actions.count());
}

// ---------------------------------------------------------------------------------------------------------------------

QString TurnoutEvent::name() const
{
    return tr("Turnout");
}

QList<Parameter> TurnoutEvent::parameters() const
{
    return {
        Parameter::number<dcc::AccessoryAddress>("primaryAddress", LMRS_TR("Primary address"), Parameter::Flag::Optional),
        Parameter::choice<dcc::TurnoutState>("primaryState", LMRS_TR("Primary state"), Parameter::Flag::Optional),
        Parameter::number<dcc::AccessoryAddress>("secondaryAddress", LMRS_TR("Secondary address"), Parameter::Flag::Optional),
        Parameter::choice<dcc::TurnoutState>("secondaryState", LMRS_TR("Secondary state"), Parameter::Flag::Optional),
    };
}

void TurnoutEvent::setPrimaryAddress(dcc::AccessoryAddress newAddress)
{
    if (std::exchange(m_primaryAddress, std::move(newAddress)) != m_primaryAddress)
        emit primaryAddressChanged(m_primaryAddress.value(), {});
}

void TurnoutEvent::resetPrimaryAddress()
{
    if (std::exchange(m_primaryAddress, {}) != m_primaryAddress)
        emit primaryAddressChanged({}, {});
}

dcc::AccessoryAddress TurnoutEvent::primaryAddress() const
{
    if (m_primaryAddress.has_value())
        return m_primaryAddress.value();

    return {};
}

bool TurnoutEvent::hasPrimaryAddress() const
{
    return m_primaryAddress.has_value();
}

void TurnoutEvent::setSecondaryAddress(dcc::AccessoryAddress newAddress)
{
    if (std::exchange(m_secondaryAddress, std::move(newAddress)) != m_secondaryAddress)
        emit secondaryAddressChanged(m_secondaryAddress.value(), {});
}

void TurnoutEvent::resetSecondaryAddress()
{
    if (std::exchange(m_secondaryAddress, {}) != m_secondaryAddress)
        emit secondaryAddressChanged({}, {});
}

dcc::AccessoryAddress TurnoutEvent::secondaryAddress() const
{
    if (m_secondaryAddress.has_value())
        return m_secondaryAddress.value();

    return {};
}

bool TurnoutEvent::hasSecondaryAddress() const
{
    return m_secondaryAddress.has_value();
}

void TurnoutEvent::setPrimaryState(dcc::TurnoutState newState)
{
    if (std::exchange(m_primaryState, std::move(newState)) != m_primaryState)
        emit primaryStateChanged(m_primaryState.value(), {});
}

void TurnoutEvent::resetPrimaryState()
{
    if (std::exchange(m_primaryState, {}) != m_primaryState)
        emit primaryStateChanged({}, {});
}

dcc::TurnoutState TurnoutEvent::primaryState() const
{
    if (m_primaryState.has_value())
        return m_primaryState.value();

    return dcc::TurnoutState::Invalid;
}

bool TurnoutEvent::hasPrimaryState() const
{
    return m_primaryState.has_value();
}

void TurnoutEvent::setSecondaryState(dcc::TurnoutState newState)
{
    if (std::exchange(m_secondaryState, std::move(newState)) != m_secondaryState)
        emit secondaryStateChanged(m_secondaryState.value(), {});
}

void TurnoutEvent::resetSecondaryState()
{
    if (std::exchange(m_secondaryState, {}) != m_secondaryState)
        emit secondaryStateChanged({}, {});
}

dcc::TurnoutState TurnoutEvent::secondaryState() const
{
    if (m_secondaryState.has_value())
        return m_secondaryState.value();

    return dcc::TurnoutState::Invalid;
}

bool TurnoutEvent::hasSecondaryState() const
{
    return m_secondaryState.has_value();
}

// ---------------------------------------------------------------------------------------------------------------------

QList<Parameter> DetectorEvent::parameters() const
{
    return QList {
// FIXME       Parameter::list<dcc::VehicleAddress>("vehicles"_L1, LMRS_TR("Vehicles")),
        Parameter::choice<Type>("type", LMRS_TR("Event")),
    };
}

void DetectorEvent::setVehicles(QList<dcc::VehicleAddress> newVehicles)
{
    if (std::exchange(m_vehicles, std::move(newVehicles)) != m_vehicles)
        emit vehiclesChanged(m_vehicles, {});
}

QList<dcc::VehicleAddress> DetectorEvent::vehicles() const
{
    return m_vehicles;
}

void DetectorEvent::setType(Type newType)
{
    if (std::exchange(m_type, std::move(newType)) != m_type)
        emit typeChanged(m_type, {});
}

DetectorEvent::Type DetectorEvent::type() const
{
    return m_type;
}

// ---------------------------------------------------------------------------------------------------------------------

QString CanDetectorEvent::name() const
{
    return tr("Detector (CAN)");
}

QList<Parameter> CanDetectorEvent::parameters() const
{
    return QList {
        Parameter::number<can::NetworkId>("network", LMRS_TR("Network"), Parameter::Flag::Hexadecimal | Parameter::Flag::Optional),
        Parameter::number<can::ModuleId>("module", LMRS_TR("Module"), Parameter::Flag::Hexadecimal | Parameter::Flag::Optional),
        Parameter::number<can::PortIndex>("port", LMRS_TR("Port"), Parameter::Flag::Optional),
    } + DetectorEvent::parameters();
}

DetectorAddress CanDetectorEvent::detector() const
{
    if (m_network.has_value()) {
        if (m_module.has_value())
            return DetectorAddress::forCanModule(m_network.value(), m_module.value());
        else
            return DetectorAddress::forCanNetwork(m_network.value());
    }

    return {};
}

bool CanDetectorEvent::hasDetector() const
{
    return m_network.has_value();
}

void CanDetectorEvent::setNetwork(can::NetworkId newNetwork)
{
    if (std::exchange(m_network, std::move(newNetwork)) != m_network)
        emit networkChanged(m_network.value(), {});
}

void CanDetectorEvent::resetNetwork()
{
    const auto detectorGuard = propertyGuard(this, &DetectorEvent::detector, &DetectorEvent::detectorChanged);

    if (std::exchange(m_network, {}) != m_network)
        emit networkChanged({}, {});
}

can::NetworkId CanDetectorEvent::network() const
{
    if (m_network.has_value())
        return m_network.value();

    return {};
}

bool CanDetectorEvent::hasNetwork() const
{
    return m_network.has_value();
}

void CanDetectorEvent::setModule(can::ModuleId newModule)
{
    if (std::exchange(m_module, std::move(newModule)) != m_module)
        emit moduleChanged(m_module.value(), {});
}

void CanDetectorEvent::resetModule()
{
    const auto detectorGuard = propertyGuard(this, &DetectorEvent::detector, &DetectorEvent::detectorChanged);

    if (std::exchange(m_module, {}) != m_module)
        emit moduleChanged({}, {});
}

can::ModuleId CanDetectorEvent::module() const
{
    if (m_module.has_value())
        return m_module.value();

    return {};
}

bool CanDetectorEvent::hasModule() const
{
    return m_module.has_value();
}

void CanDetectorEvent::setPort(can::PortIndex newPort)
{
    if (std::exchange(m_port, std::move(newPort)) != m_port)
        emit portChanged(m_port.value(), {});
}

void CanDetectorEvent::resetPort()
{
    if (std::exchange(m_port, {}) != m_port)
        emit portChanged({}, {});
}

can::PortIndex CanDetectorEvent::port() const
{
    if (m_port.has_value())
        return m_port.value();

    return {};
}

bool CanDetectorEvent::hasPort() const
{
    return m_port.has_value();
}

// ---------------------------------------------------------------------------------------------------------------------

QString RBusDetectorGroupEvent::name() const
{
    return tr("Detector Group (RBus)");
}

QList<Parameter> RBusDetectorGroupEvent::parameters() const
{
    return QList {
        Parameter::number<rbus::GroupId>("group", LMRS_TR("Group")),
    } + DetectorEvent::parameters();
}

DetectorAddress RBusDetectorGroupEvent::detector() const
{
    if (m_group.has_value())
        return DetectorAddress::forRBusGroup(m_group.value());

    return {};
}

bool RBusDetectorGroupEvent::hasDetector() const
{
    return m_group.has_value();
}

void RBusDetectorGroupEvent::setGroup(rbus::GroupId newGroup)
{
    if (std::exchange(m_group, std::move(newGroup)) != m_group)
        emit groupChanged(m_group.value(), {});
}

void RBusDetectorGroupEvent::resetGroup()
{
    if (std::exchange(m_group, {}) != m_group)
        emit groupChanged({}, {});
}

rbus::GroupId RBusDetectorGroupEvent::group() const
{
    if (m_group.has_value())
        return m_group.value();

    return {};
}

bool RBusDetectorGroupEvent::hasGroup() const
{
    return m_group.has_value();
}

// ---------------------------------------------------------------------------------------------------------------------

QString RBusDetectorEvent::name() const
{
    return tr("Detector (RBus)");
}

QList<Parameter> RBusDetectorEvent::parameters() const
{
    return QList {
        Parameter::number<rbus::ModuleId>("module", LMRS_TR("Module"), Parameter::Flag::Optional),
        Parameter::number<rbus::PortIndex>("port", LMRS_TR("Port"), Parameter::Flag::Optional),
    } + DetectorEvent::parameters();
}

DetectorAddress RBusDetectorEvent::detector() const
{
    if (m_module.has_value()) {
        if (m_port.has_value())
            return DetectorAddress::forRBusPort(m_module.value(), m_port.value());
        else
            return DetectorAddress::forRBusGroup(group(m_module.value()));
    }

    return {};
}

bool RBusDetectorEvent::hasDetector() const
{
    return m_module.has_value();
}

void RBusDetectorEvent::setModule(rbus::ModuleId newModule)
{
    if (std::exchange(m_module, std::move(newModule)) != m_module)
        emit moduleChanged(m_module.value(), {});
}

void RBusDetectorEvent::resetModule()
{
    if (std::exchange(m_module, {}) != m_module)
        emit moduleChanged({}, {});
}

rbus::ModuleId RBusDetectorEvent::module() const
{
    if (m_module.has_value())
        return m_module.value();

    return {};
}

bool RBusDetectorEvent::hasModule() const
{
    return m_module.has_value();
}

void RBusDetectorEvent::setPort(rbus::PortIndex newPort)
{
    if (std::exchange(m_port, std::move(newPort)) != m_port)
        emit portChanged(m_port.value(), {});
}

void RBusDetectorEvent::resetPort()
{
    if (std::exchange(m_port, {}) != m_port)
        emit portChanged({}, {});
}

rbus::PortIndex RBusDetectorEvent::port() const
{
    if (m_port.has_value())
        return m_port.value();

    return {};
}

bool RBusDetectorEvent::hasPort() const
{
    return m_port.has_value();
}

// =====================================================================================================================

QString TurnoutAction::name() const
{
    return tr("Turnout");
}

QList<Parameter> TurnoutAction::parameters() const
{
    return {
        Parameter::number<dcc::AccessoryAddress>("address", LMRS_TR("Address"), Parameter::Flag::Optional),
        Parameter::choice<dcc::TurnoutState>("state", LMRS_TR("State"), Parameter::Flag::Optional),
//                Parameter::flag(s_detail_event_isPrimary, LMRS_TR("Primary Match")),
//                Parameter::flag(s_detail_event_isSecondary, LMRS_TR("Secondary Match")),
    };
}

void TurnoutAction::setAddress(dcc::AccessoryAddress newAddress)
{
    if (std::exchange(m_address, std::move(newAddress)) != m_address)
        emit addressChanged(m_address.value(), {});
}

void TurnoutAction::resetAddress()
{
    if (std::exchange(m_address, {}) != m_address)
        emit addressChanged({}, {});
}

dcc::AccessoryAddress TurnoutAction::address() const
{
    if (m_address.has_value())
        return m_address.value();

    return {};
}

bool TurnoutAction::hasAddress() const
{
    return m_address.has_value();
}

void TurnoutAction::setState(dcc::TurnoutState newState)
{
    if (std::exchange(m_state, std::move(newState)) != m_state)
        emit stateChanged(m_state.value(), {});
}

void TurnoutAction::resetState()
{
    if (std::exchange(m_state, {}) != m_state)
        emit stateChanged({}, {});
}

dcc::TurnoutState TurnoutAction::state() const
{
    if (m_state.has_value())
        return m_state.value();

    return {};
}

bool TurnoutAction::hasState() const
{
    return m_state.has_value();
}

// ---------------------------------------------------------------------------------------------------------------------

QString VehicleAction::name() const
{
    return tr("Vehicle");
}

QList<Parameter> VehicleAction::parameters() const
{
    return {
        Parameter::number<dcc::VehicleAddress>("address", LMRS_TR("Address"), Parameter::Flag::Optional),
        Parameter::number<dcc::Speed126>("speed", LMRS_TR("Speed"), Parameter::Flag::Optional),
        Parameter::choice<dcc::Direction>("direction", LMRS_TR("Direction"), Parameter::Flag::Optional),
// FIXME        Parameter::bitmask<dcc::FunctionState>("functions", LMRS_TR("Functions"), Parameter::Flag::Optional),
    };
}

void VehicleAction::setAddress(dcc::VehicleAddress newAddress)
{
    if (std::exchange(m_address, std::move(newAddress)) != m_address)
        emit addressChanged(m_address.value(), {});
}

void VehicleAction::resetAddress()
{
    if (std::exchange(m_address, {}) != m_address)
        emit addressChanged({}, {});
}

dcc::VehicleAddress VehicleAction::address() const
{
    if (m_address.has_value())
        return m_address.value();

    return {};
}

bool VehicleAction::hasAddress() const
{
    return m_address.has_value();
}

dcc::Speed126 VehicleAction::speed() const
{
    if (m_speed.has_value())
        return m_speed.value();

    return {};
}

bool VehicleAction::hasSpeed() const
{
    return m_speed.has_value();
}

void VehicleAction::setSpeed(dcc::Speed126 newSpeed)
{
    if (std::exchange(m_speed, std::move(newSpeed)) != m_speed)
        emit speedChanged(m_speed.value(), {});
}

void VehicleAction::resetSpeed()
{
    if (std::exchange(m_speed, {}) != m_speed)
        emit speedChanged({}, {});
}

dcc::Direction VehicleAction::direction() const
{
    if (m_direction.has_value())
        return m_direction.value();

    return {};
}

bool VehicleAction::hasDirection() const
{
    return m_direction.has_value();
}

void VehicleAction::setDirection(dcc::Direction newDirection)
{
    if (std::exchange(m_direction, std::move(newDirection)) != m_direction)
        emit directionChanged(m_direction.value(), {});
}

void VehicleAction::resetDirection()
{
    if (std::exchange(m_direction, {}) != m_direction)
        emit directionChanged({}, {});
}

dcc::FunctionState VehicleAction::functions() const
{
    if (m_functions.has_value())
        return m_functions.value();

    return {};
}

bool VehicleAction::hasFunctions() const
{
    return m_functions.has_value();
}

void VehicleAction::setFunctionMask(quint64 newFunctions)
{
    setFunctions(dcc::FunctionState{newFunctions});
}

quint64 VehicleAction::functionMask() const
{
    return functions().to_ullong();
}

void VehicleAction::setFunctions(dcc::FunctionState newFunctions)
{
    if (std::exchange(m_functions, std::move(newFunctions)) != m_functions)
        emit functionsChanged(m_functions.value(), {});
}

void VehicleAction::resetFunctions()
{
    if (std::exchange(m_functions, {}) != m_functions)
        emit functionsChanged({}, {});
}

// ---------------------------------------------------------------------------------------------------------------------

QString MessageAction::name() const
{
    return tr("Log Message");
}

QList<Parameter> MessageAction::parameters() const
{
    return {
        // FIXME: provide advice regarding placeholders: "Turnout {primaryAddress}{secondaryAddress:/{secondaryAddress}} has switched to {primaryState}{secondaryState:, and {secondaryState}}"
        Parameter::text("message", LMRS_TR("Message")),
    };
}

void MessageAction::setMessage(QString newMessage)
{
    if (std::exchange(m_message, std::move(newMessage)) != m_message)
        emit messageChanged(m_message, {});
}

QString MessageAction::message() const
{
    return m_message;
}

// =====================================================================================================================

AutomationTypeModel::Row::Row(l10n::String text)
    : m_value{std::move(text)}
{}

AutomationTypeModel::Row::Row(ItemPointer event)
    : m_value{std::move(event)}
{}

Item *AutomationTypeModel::Row::item() const
{
    if (const auto event = std::get_if<ItemPointer>(&m_value))
        return event->get();

    return {};
}

std::optional<l10n::String> AutomationTypeModel::Row::text() const
{
    if (const auto text = std::get_if<l10n::String>(&m_value))
        return *text;

    return {};
}

// ---------------------------------------------------------------------------------------------------------------------

AutomationTypeModel::AutomationTypeModel(QObject *parent)
    : QAbstractListModel{parent}
{
    m_rows.emplaceBack(LMRS_TR("Events"));

    registerType<TurnoutEvent>();
    registerType<CanDetectorEvent>();
    registerType<RBusDetectorGroupEvent>();
    registerType<RBusDetectorEvent>();

    m_rows.emplaceBack(LMRS_TR("Actions"));

    registerType<MessageAction>();
    registerType<TurnoutAction>();
    registerType<VehicleAction>();
}

Item *AutomationTypeModel::fromMetaType(QMetaType type, QObject *parent) const
{
    if (const auto factory = m_factories.value(type.id()))
        return factory(parent);

    qCWarning(logger(this), "Could not create item of type %s", type.name());
    return nullptr;
}

Item *AutomationTypeModel::fromMetaObject(const QMetaObject *metaObject, QObject *parent) const
{
    return fromMetaType(metaTypeFromClassName(metaObject->className()), parent);
}

Item *AutomationTypeModel::fromJsonObject(QMetaType baseType, QJsonObject object, QObject *parent) const
{
    const auto typeUri = object[s_schema].toString();
    const auto type = metaTypeFromUri(QUrl{typeUri});

    if (type.metaObject() && type.metaObject()->inherits(baseType.metaObject())) {
        if (const auto item = fromMetaType(type, parent)) {
            for (auto it = object.begin(); it != object.end(); ++it) {
                const auto key = it.key().toLatin1();

                if (it.key().startsWith('$'_L1))
                    continue;

                if (const auto p = item->parameter(key)) {
                    item->setProperty(key, p->fromJson(it.value()));
                } else {
                    qCWarning(logger(this), "Skipping unexpected property \"%s\"", key.constData());
                }
            }

            return item;
        }
    }

    qCWarning(logger(this), "Unsupported automation type: %ls", qUtf16Printable(typeUri));
    return nullptr;
}

Item *AutomationTypeModel::fromJson(QMetaType baseType, QByteArray json, QObject *parent) const
{
    auto parseStatus = QJsonParseError{};
    const auto document = QJsonDocument::fromJson(std::move(json), &parseStatus);

    if (parseStatus.error != QJsonParseError::NoError) {
        qCWarning(logger(this), "%ls", qUtf16Printable(parseStatus.errorString()));
        return nullptr;
    }

    return fromJsonObject(std::move(baseType), document.object(), parent);
}

void AutomationTypeModel::registerType(int typeId, Factory createEvent)
{
    auto prototype = Row::ItemPointer{createEvent(this)};
    m_factories.insert(typeId, std::move(createEvent));

    for (const auto &p: prototype->parameters()) {
        if (!verifyProperty(prototype.get(), p, p.valueKey()))
            continue;

        if (p.flags() & Parameter::Flag::Optional && !verifyProperty(prototype.get(), p, p.hasValueKey()))
            continue;
    }

    const auto newRow = static_cast<int>(m_rows.size());
    beginInsertRows({}, newRow, newRow);
    m_rows.emplaceBack(std::move(prototype));
    endInsertRows();
}

bool AutomationTypeModel::verifyProperty(const Item *target, const Parameter &parameter, QByteArrayView propertyName)
{
    const auto propertyIndex = target->metaObject()->indexOfProperty(propertyName.constData());
    const auto property = target->metaObject()->property(propertyIndex);

    if (!property.isValid()) {
        qCCritical(logger<AutomationModel>(),
                   "Property \"%s\" not found for parameter \"%s\" in automation type %s",
                   propertyName.constData(), parameter.key().constData(), target->metaObject()->className());

        return false;
    }

    if (propertyName == parameter.valueKey()) {
        if (!parameter.acceptsType(property.metaType())) {
            qCCritical(logger<AutomationModel>(),
                       "Type \"%s\" of property \"%s\" is not accepted by parameter \"%s\" in automation type %s",
                       property.typeName(), propertyName.constData(), parameter.key().constData(),
                       target->metaObject()->className());

            return false;
        }
    } else if (propertyName == parameter.hasValueKey()) {
        if (property.typeId() != qMetaTypeId<bool>()) {
            qCCritical(logger<AutomationModel>(),
                       "Type \"%s\" of property \"%s\" should be \"bool\" for parameter \"%s\" in automation type %s",
                       property.typeName(), propertyName.constData(), parameter.key().constData(),
                       target->metaObject()->className());

            return false;
        }
    } else {
        Q_UNREACHABLE();
    }

    return true;
}

int AutomationTypeModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_rows.count());
}

QVariant AutomationTypeModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (static_cast<DataRole>(role)) {
        case NameRole:
            if (const auto text = m_rows[index.row()].text())
                return text->toString();
            if (const auto item = m_rows[index.row()].item())
                return item->name();

            break;

        case ActionRole:
            if (const auto action = m_rows[index.row()].action())
                return QVariant::fromValue(action);

            break;

        case EventRole:
            if (const auto event = m_rows[index.row()].event())
                return QVariant::fromValue(event);

            break;

        case ItemRole:
            if (const auto item = m_rows[index.row()].item())
                return QVariant::fromValue(item);

            break;
        }
    }

    return {};
}

Qt::ItemFlags AutomationTypeModel::flags(const QModelIndex &index) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        if (m_rows[index.row()].item())
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren;
        if (m_rows[index.row()].text())
            return Qt::ItemNeverHasChildren;
    }

    return Qt::ItemFlags{};
}

QHash<int, QByteArray> AutomationTypeModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {ActionRole, "action"},
        {EventRole, "event"},
        {ItemRole, "item"},
    };
}

const Action *AutomationTypeModel::actionItem(const QModelIndex &index) const
{
    if (const auto proxyModel = dynamic_cast<const QAbstractProxyModel *>(index.model()))
        return actionItem(proxyModel->mapToSource(index));
    if (LMRS_FAILED_EQUALS(logger(this), index.model(), this))
        return nullptr;

    return qvariant_cast<const Action *>(index.data(ActionRole));
}

const Event *AutomationTypeModel::eventItem(const QModelIndex &index) const
{
    if (const auto proxyModel = dynamic_cast<const QAbstractProxyModel *>(index.model()))
        return eventItem(proxyModel->mapToSource(index));
    if (LMRS_FAILED_EQUALS(logger(this), index.model(), this))
        return nullptr;

    return qvariant_cast<const Event *>(index.data(EventRole));
}

const Item *AutomationTypeModel::item(const QModelIndex &index) const
{
    if (const auto proxyModel = dynamic_cast<const QAbstractProxyModel *>(index.model()))
        return item(proxyModel->mapToSource(index));
    if (LMRS_FAILED_EQUALS(logger(this), index.model(), this))
        return nullptr;

    return qvariant_cast<const Item *>(index.data(ItemRole));
}

// =====================================================================================================================

int AutomationModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_events.count());
}

QVariant AutomationModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (static_cast<DataRole>(role)) {
        case NameRole:
            return m_events[index.row()]->name();

        case EventRole:
            return QVariant::fromValue(m_events[index.row()]);
        }
    }

    return {};
}

const Event *AutomationModel::eventItem(const QModelIndex &index) const
{
    if (!index.isValid() || LMRS_FAILED_EQUALS(logger(this), index.model(), this))
        return nullptr;

    return qvariant_cast<const Event *>(index.data(EventRole));
}

Event *AutomationModel::eventItem(const QModelIndex &index)
{
    if (!index.isValid() || LMRS_FAILED_EQUALS(logger(this), index.model(), this))
        return nullptr;

    return qvariant_cast<Event *>(index.data(EventRole));
}

void AutomationModel::observeItem(Item *item, const QMetaObject *baseType, QMetaMethod observer)
{
    static constexpr auto DirectUniqueConnection = static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection);

    for (auto i = baseType->propertyOffset(); i < item->metaObject()->propertyCount(); ++i) {
        const auto notifySignal = item->metaObject()->property(i).notifySignal();
        const auto notifySignature = notifySignal.methodSignature();

        if (!notifySignature.isEmpty())
            connect(item, notifySignal, this, observer, DirectUniqueConnection);
    }
}

QHash<int, QByteArray> AutomationModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {EventRole, "event"},
    };
}

int AutomationModel::appendEvent(Event *event)
{
    return insertEvent(event, rowCount());
}

int AutomationModel::insertEvent(Event *event, int before)
{
    if (LMRS_FAILED(logger(this), event != nullptr)
            || LMRS_FAILED_COMPARE(logger(this), before, >=, 0)
            || LMRS_FAILED_COMPARE(logger(this), before, <=, rowCount()))
        return -1;

    beginInsertRows({}, before, before);

    event->setParent(this);
    m_events.emplace(before, event);

    const auto observeAction = [this](Action *action) {
        observeItem(action, &Action::staticMetaObject, LMRS_CORE_FIND_META_METHOD(&AutomationModel::onActionChanged));
    };

    observeItem(event, &Event::staticMetaObject, LMRS_CORE_FIND_META_METHOD(&AutomationModel::onEventChanged));

    for (const auto action: event->actions())
        observeAction(action);

    connect(event, &Event::actionInserted, this, observeAction);
    connect(event, &Event::actionRemoved, this, [this](Action *action) {
        if (action)
            action->disconnect(this);
    });

    endInsertRows();

    return before;
}

int AutomationModel::appendAction(const QModelIndex &index, Action *action)
{
    const auto event = eventItem(index);

    if (LMRS_FAILED(logger(this), event != nullptr))
        return -1;

    return event->appendAction(action);
}

int AutomationModel::insertAction(const QModelIndex &index, Action *action, int before)
{
    const auto event = eventItem(index);

    if (LMRS_FAILED(logger(this), event != nullptr))
        return -1;

    return event->insertAction(action, before);
}

bool AutomationModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (LMRS_FAILED(logger(this), !parent.isValid())
            || LMRS_FAILED_COMPARE(logger(this), count, >=, 1)
            || LMRS_FAILED_COMPARE(logger(this), row + count, <=, rowCount()))
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    const auto begin = m_events.begin() + row;
    const auto end = begin + count;

    qDeleteAll(begin, end);
    m_events.erase(begin, end);

    endRemoveRows();

    return true;
}

void AutomationModel::clear()
{
    beginResetModel();
    qDeleteAll(m_events);
    m_events.clear();
    endResetModel();
}

void AutomationModel::onActionChanged()
{
    QMetaObject::invokeMethod(this, [this, action = dynamic_cast<Action *>(sender())] {
        emit actionChanged(action);
    });
}

void AutomationModel::onEventChanged()
{
    QMetaObject::invokeMethod(this, [this, event = dynamic_cast<Event *>(sender())] {
        emit eventChanged(event);
    });
}

// =====================================================================================================================

JsonFileReader::ModelPointer JsonFileReader::read(const AutomationTypeModel *types)
{
    if (LMRS_FAILED(logger(this), types != nullptr))
        return {};

    auto file = QFile{fileName()};

    if (!file.open(QFile::ReadOnly)) {
        reportError(file.errorString());
        return {};
    }

    auto parseStatus = QJsonParseError{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseStatus);

    if (parseStatus.error != QJsonParseError::NoError) {
        reportError(parseStatus.errorString());
        return {};
    }

    const auto root = document.object();

    if (QUrl{root[s_schema].toString()} != typeUri<AutomationModel>()) {
        reportError(tr("Unexpected file format."));
        return {};
    }

    auto model = std::make_unique<AutomationModel>();

    for (const auto eventArray = root[s_events].toArray(); const auto &value: eventArray) {
        if (const auto event = types->fromJsonObject<Event>(value.toObject(), model.get())) {
            for (const auto actionArray = value[s_actions].toArray(); const auto &value: actionArray) {
                if (const auto action = types->fromJsonObject<Action>(value.toObject(), model.get()))
                    event->appendAction(action);
            }

            model->appendEvent(event);
        }
    }

    return model;
}

bool JsonFileWriter::write(const AutomationModel *model)
{
    auto events = QJsonArray{};

    for (const auto &index: model) {
        if (const auto event = qvariant_cast<const Event *>(index.data(AutomationModel::EventRole)))
            events.append(event->toJsonObject());
    }

    auto file = QFile{fileName()};

    if (!file.open(QFile::WriteOnly)) {
        reportError(file.errorString());
        return false;
    }

    auto root = QJsonObject{
        {s_schema, typeUri<AutomationModel>().toString()},
        {s_events, std::move(events)}
    };

    auto json = QJsonDocument{std::move(root)}.toJson();
    const auto bytesToWrite = json.length();

    if (file.write(std::move(json)) != bytesToWrite) {
        reportError(file.errorString());
        return false;
    }

    return true;
}

// =====================================================================================================================

} // namespace lmrs::core::automation

template<>
lmrs::core::automation::AutomationModelReader::Registry &
lmrs::core::automation::AutomationModelReader::registry()
{
    static auto formats = Registry {
        {FileFormat::lmrsAutomationModel(), std::make_unique<automation::JsonFileReader, QString>},
    };

    return formats;
}

template<>
lmrs::core::automation::AutomationModelWriter::Registry &
lmrs::core::automation::AutomationModelWriter::registry()
{
    static auto formats = Registry {
        {FileFormat::lmrsAutomationModel(), std::make_unique<automation::JsonFileWriter, QString>},
    };

    return formats;
}

// =====================================================================================================================

#include "automationmodel.moc"
#include "moc_automationmodel.cpp"
