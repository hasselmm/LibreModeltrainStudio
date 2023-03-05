#ifndef LMRS_CORE_AUTOMATIONMODEL_H
#define LMRS_CORE_AUTOMATIONMODEL_H

#include "dccconstants.h"
#include "detectors.h"
#include "fileformat.h"
#include "localization.h"

#include <QAbstractListModel>

namespace lmrs::core {
class Device;

class AccessoryControl;
class DebugControl;
class DetectorControl;
class PowerControl;
class SpeedMeterControl;
class VariableControl;
class VehicleControl;

namespace accessory {
struct DetectorInfo;
struct TurnoutInfo;
}

namespace parameters {
struct Parameter;
}

}

namespace lmrs::core::automation {

using parameters::Parameter;

// =====================================================================================================================

class Item : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY nameChanged FINAL)
    Q_PROPERTY(QList<lmrs::core::Parameter> parameters READ parameters CONSTANT FINAL)

public:
    using QObject::QObject;

    virtual QString name() const = 0;
    virtual QList<Parameter> parameters() const = 0;
    virtual QJsonObject toJsonObject() const;

    QByteArray toJson() const;

    std::optional<Parameter> parameter(QByteArrayView key) const;

signals:
    void nameChanged(QString name, QPrivateSignal);
};

// ---------------------------------------------------------------------------------------------------------------------

template<class T>
concept ItemType = std::is_base_of_v<Item, T>;

// =====================================================================================================================

class Action : public Item
{
    Q_OBJECT

public:
    using Item::Item;
};

// ---------------------------------------------------------------------------------------------------------------------

class TurnoutAction : public Action
{
    Q_OBJECT

    Q_PROPERTY(quint16 address READ address WRITE setAddress RESET resetAddress NOTIFY addressChanged FINAL)
    Q_PROPERTY(bool hasAddress READ hasAddress NOTIFY addressChanged FINAL)

    Q_PROPERTY(core::dcc::TurnoutState state READ state WRITE setState RESET resetState NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool hasState READ hasState NOTIFY stateChanged FINAL)

public:
    using Action::Action;

    QString name() const override;
    QList<Parameter> parameters() const override;

    void setAddress(dcc::AccessoryAddress newAddress);
    dcc::AccessoryAddress address() const;
    bool hasAddress() const;

    void setState(dcc::TurnoutState newState);
    dcc::TurnoutState state() const;
    bool hasState() const;

public slots:
    void resetAddress();
    void resetState();

signals:
    void addressChanged(dcc::AccessoryAddress address, QPrivateSignal);
    void stateChanged(dcc::TurnoutState state, QPrivateSignal);

private:
    std::optional<dcc::AccessoryAddress> m_address;
    std::optional<dcc::TurnoutState> m_state;
};

// ---------------------------------------------------------------------------------------------------------------------

class VehicleAction : public Action
{
    Q_OBJECT

    Q_PROPERTY(quint16 address READ address WRITE setAddress RESET resetAddress NOTIFY addressChanged FINAL)
    Q_PROPERTY(bool hasAddress READ hasAddress NOTIFY addressChanged FINAL)

    Q_PROPERTY(quint8 speed READ speed WRITE setSpeed RESET resetSpeed NOTIFY speedChanged FINAL)
    Q_PROPERTY(bool hasSpeed READ hasSpeed NOTIFY speedChanged FINAL)

    Q_PROPERTY(lmrs::core::dcc::Direction direction READ direction WRITE setDirection RESET resetDirection NOTIFY directionChanged FINAL)
    Q_PROPERTY(bool hasDirection READ hasDirection NOTIFY directionChanged FINAL)

    Q_PROPERTY(lmrs::core::dcc::FunctionState functions READ functions WRITE setFunctions RESET resetFunctions NOTIFY functionsChanged FINAL)
    Q_PROPERTY(quint64 functionMask READ functionMask WRITE setFunctionMask RESET resetFunctions NOTIFY functionsChanged FINAL)
    Q_PROPERTY(bool hasFunctions READ hasFunctions NOTIFY functionsChanged FINAL)

public:
    using Action::Action;

    QString name() const override;
    QList<Parameter> parameters() const override;

    void setAddress(dcc::VehicleAddress newAddress);
    dcc::VehicleAddress address() const;
    bool hasAddress() const;

    void setSpeed(dcc::Speed126 newSpeed);
    dcc::Speed126 speed() const;
    bool hasSpeed() const;

    void setDirection(dcc::Direction newDirection);
    dcc::Direction direction() const;
    bool hasDirection() const;

    void setFunctions(dcc::FunctionState newFunctions);
    dcc::FunctionState functions() const;
    bool hasFunctions() const;

    void setFunctionMask(quint64 newFunctions);
    quint64 functionMask() const;

public slots:
    void resetAddress();
    void resetSpeed();
    void resetDirection();
    void resetFunctions();

signals:
    void addressChanged(quint16 address, QPrivateSignal);
    void speedChanged(quint8 address, QPrivateSignal);
    void directionChanged(lmrs::core::dcc::Direction direction, QPrivateSignal);
    void functionsChanged(lmrs::core::dcc::FunctionState functions, QPrivateSignal);

private:
    std::optional<dcc::VehicleAddress> m_address;
    std::optional<dcc::Speed126> m_speed; // FIXME: allow dcc::Speed
    std::optional<dcc::Direction> m_direction;
    std::optional<dcc::FunctionState> m_functions;
};

// ---------------------------------------------------------------------------------------------------------------------

class MessageAction : public Action
{
    Q_OBJECT
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged FINAL)

public:
    using Action::Action;

    QString name() const override;
    QList<Parameter> parameters() const override;

    void setMessage(QString newMessage);
    QString message() const;

signals:
    void messageChanged(QString message, QPrivateSignal);

private:
    QString m_message;
};

// =====================================================================================================================

class Event : public Item
{
    Q_OBJECT
    Q_PROPERTY(QList<lmrs::core::automation::Action *> actions READ actions NOTIFY actionsChanged FINAL)

public:
    using Item::Item;

    QJsonObject toJsonObject() const override;

    int appendAction(Action *action);
    int insertAction(Action *action, int before);
    Action *removeAction(int index);

    QList<Action *> actions() const;
    int actionCount() const;

signals:
    void actionInserted(lmrs::core::automation::Action *action, int index, QPrivateSignal);
    void actionRemoved(lmrs::core::automation::Action *action, int index, QPrivateSignal);
    void actionsChanged(QPrivateSignal);

protected:
    virtual void setControl(AccessoryControl *) {}
    virtual void setControl(DebugControl *) {}
    virtual void setControl(DetectorControl *) {}
    virtual void setControl(PowerControl *) {}
    virtual void setControl(SpeedMeterControl *) {}
    virtual void setControl(VariableControl *) {}
    virtual void setControl(VehicleControl *) {}

private:
    friend class AutomationModel;

    QList<Action *> m_actions;
};

// ---------------------------------------------------------------------------------------------------------------------

class TurnoutEvent : public Event
{
    Q_OBJECT

    Q_PROPERTY(quint16 primaryAddress READ primaryAddress WRITE setPrimaryAddress RESET resetPrimaryAddress NOTIFY primaryAddressChanged FINAL)
    Q_PROPERTY(bool hasPrimaryAddress READ hasPrimaryAddress NOTIFY primaryAddressChanged FINAL)

    Q_PROPERTY(core::dcc::TurnoutState primaryState READ primaryState WRITE setPrimaryState RESET resetPrimaryState NOTIFY primaryStateChanged FINAL)
    Q_PROPERTY(bool hasPrimaryState READ hasPrimaryState NOTIFY primaryStateChanged FINAL)

    Q_PROPERTY(quint16 secondaryAddress READ secondaryAddress WRITE setSecondaryAddress NOTIFY secondaryAddressChanged FINAL)
    Q_PROPERTY(bool hasSecondaryAddress READ hasSecondaryAddress NOTIFY secondaryAddressChanged FINAL)

    Q_PROPERTY(core::dcc::TurnoutState secondaryState READ secondaryState WRITE setSecondaryState NOTIFY secondaryStateChanged FINAL)
    Q_PROPERTY(bool hasSecondaryState READ hasSecondaryState NOTIFY secondaryStateChanged FINAL)

public:
    using Event::Event;

    QString name() const override;
    QList<Parameter> parameters() const override;

    void setPrimaryAddress(dcc::AccessoryAddress newAddress);
    dcc::AccessoryAddress primaryAddress() const;
    bool hasPrimaryAddress() const;

    void setSecondaryAddress(dcc::AccessoryAddress newAddress);
    dcc::AccessoryAddress secondaryAddress() const;
    bool hasSecondaryAddress() const;

    void setPrimaryState(dcc::TurnoutState newState);
    dcc::TurnoutState primaryState() const;
    bool hasPrimaryState() const;

    void setSecondaryState(dcc::TurnoutState newState);
    dcc::TurnoutState secondaryState() const;
    bool hasSecondaryState() const;

public slots:
    void resetPrimaryAddress();
    void resetSecondaryAddress();
    void resetPrimaryState();
    void resetSecondaryState();

signals:
    void primaryAddressChanged(dcc::AccessoryAddress primaryAddress, QPrivateSignal);
    void secondaryAddressChanged(dcc::AccessoryAddress secondaryAddress, QPrivateSignal);
    void primaryStateChanged(dcc::TurnoutState primaryState, QPrivateSignal);
    void secondaryStateChanged(dcc::TurnoutState secondaryState, QPrivateSignal);

protected:
    void setControl(AccessoryControl *newControl) override;

private:
    void onTurnoutInfoChanged(accessory::TurnoutInfo info);

    std::optional<dcc::AccessoryAddress> m_primaryAddress;
    std::optional<dcc::TurnoutState> m_primaryState;

    std::optional<dcc::AccessoryAddress> m_secondaryAddress;
    std::optional<dcc::TurnoutState> m_secondaryState;
};

// ---------------------------------------------------------------------------------------------------------------------

class DetectorEvent : public Event
{
    Q_OBJECT

    Q_PROPERTY(lmrs::core::accessory::DetectorAddress detector READ detector NOTIFY detectorChanged FINAL)
    Q_PROPERTY(bool hasDetector READ hasDetector NOTIFY detectorChanged FINAL)

    Q_PROPERTY(QList<lmrs::core::dcc::VehicleAddress> vehicles READ vehicles WRITE setVehicles NOTIFY vehiclesChanged FINAL)
    Q_PROPERTY(lmrs::core::automation::DetectorEvent::Type type READ type WRITE setType NOTIFY typeChanged FINAL)

public:
    enum class Type {
        Any,
        Entering,
        Leaving,
    };

    Q_ENUM(Type)

    using Event::Event;

    QList<Parameter> parameters() const override;

    virtual accessory::DetectorAddress detector() const = 0;
    virtual bool hasDetector() const = 0;

    void setVehicles(QList<dcc::VehicleAddress> newVehicles);
    QList<dcc::VehicleAddress> vehicles() const;

    void setType(Type newType);
    Type type() const;

signals:
    void detectorChanged(lmrs::core::accessory::DetectorAddress detector, QPrivateSignal);
    void vehiclesChanged(QList<lmrs::core::dcc::VehicleAddress> vehicles, QPrivateSignal);
    void typeChanged(lmrs::core::automation::DetectorEvent::Type type, QPrivateSignal);

protected:
    void setControl(DetectorControl *newControl) override;

private:
    void onDetectorInfoChanged(accessory::DetectorInfo);

    QList<dcc::VehicleAddress> m_vehicles;
    Type m_type = Type::Any;
};

// ---------------------------------------------------------------------------------------------------------------------

class CanDetectorEvent : public DetectorEvent
{
    Q_OBJECT

    Q_PROPERTY(lmrs::core::accessory::can::NetworkId network READ network WRITE setNetwork RESET resetNetwork NOTIFY networkChanged FINAL)
    Q_PROPERTY(bool hasNetwork READ hasNetwork NOTIFY networkChanged FINAL)

    Q_PROPERTY(lmrs::core::accessory::can::ModuleId module READ module WRITE setModule RESET resetModule NOTIFY moduleChanged FINAL)
    Q_PROPERTY(bool hasModule READ hasModule NOTIFY moduleChanged FINAL)

    Q_PROPERTY(lmrs::core::accessory::can::PortIndex port READ port WRITE setPort RESET resetPort NOTIFY portChanged FINAL)
    Q_PROPERTY(bool hasPort READ hasPort NOTIFY portChanged FINAL)

public:
    using DetectorEvent::DetectorEvent;

    QString name() const override;
    QList<Parameter> parameters() const override;

    accessory::DetectorAddress detector() const override;
    bool hasDetector() const override;

    void setNetwork(accessory::can::NetworkId newNetwork);
    accessory::can::NetworkId network() const;
    bool hasNetwork() const;

    void setModule(accessory::can::ModuleId newModule);
    accessory::can::ModuleId module() const;
    bool hasModule() const;

    void setPort(accessory::can::PortIndex newPort);
    accessory::can::PortIndex port() const;
    bool hasPort() const;

public slots:
    void resetNetwork();
    void resetModule();
    void resetPort();

signals:
    void networkChanged(lmrs::core::accessory::can::NetworkId network, QPrivateSignal);
    void moduleChanged(lmrs::core::accessory::can::ModuleId module, QPrivateSignal);
    void portChanged(lmrs::core::accessory::can::PortIndex port, QPrivateSignal);

protected:
    void setControl(DetectorControl *newControl) override;

private:
    void onDetectorInfoChanged(accessory::DetectorInfo info);

    std::optional<accessory::can::NetworkId> m_network;
    std::optional<accessory::can::ModuleId> m_module;
    std::optional<accessory::can::PortIndex> m_port;
};

// ---------------------------------------------------------------------------------------------------------------------

class RBusDetectorGroupEvent : public DetectorEvent
{
    Q_OBJECT

    Q_PROPERTY(lmrs::core::accessory::rbus::GroupId group READ group WRITE setGroup RESET resetGroup NOTIFY groupChanged FINAL)
    Q_PROPERTY(bool hasGroup READ hasGroup NOTIFY groupChanged FINAL)

public:
    using DetectorEvent::DetectorEvent;

    QString name() const override;
    QList<Parameter> parameters() const override;

    accessory::DetectorAddress detector() const override;
    bool hasDetector() const override;

    void setGroup(accessory::rbus::GroupId newGroup);
    accessory::rbus::GroupId group() const;
    bool hasGroup() const;

public slots:
    void resetGroup();

signals:
    void groupChanged(lmrs::core::accessory::rbus::GroupId group, QPrivateSignal);

protected:
    void setControl(DetectorControl *newControl) override;

private:
    void onDetectorInfoChanged(accessory::DetectorInfo info);

    std::optional<accessory::rbus::GroupId> m_group;
};

// ---------------------------------------------------------------------------------------------------------------------

class RBusDetectorEvent : public DetectorEvent
{
    Q_OBJECT

    Q_PROPERTY(lmrs::core::accessory::rbus::ModuleId module READ module WRITE setModule RESET resetModule NOTIFY moduleChanged FINAL)
    Q_PROPERTY(bool hasModule READ hasModule NOTIFY moduleChanged FINAL)

    Q_PROPERTY(lmrs::core::accessory::rbus::PortIndex port READ port WRITE setPort RESET resetPort NOTIFY portChanged FINAL)
    Q_PROPERTY(bool hasPort READ hasPort NOTIFY portChanged FINAL)

public:
    using DetectorEvent::DetectorEvent;

    QString name() const override;
    QList<Parameter> parameters() const override;

    accessory::DetectorAddress detector() const override;
    bool hasDetector() const override;

    void setModule(accessory::rbus::ModuleId newModule);
    accessory::rbus::ModuleId module() const;
    bool hasModule() const;

    void setPort(accessory::rbus::PortIndex newPort);
    accessory::rbus::PortIndex port() const;
    bool hasPort() const;

public slots:
    void resetModule();
    void resetPort();

signals:
    void moduleChanged(lmrs::core::accessory::rbus::ModuleId module, QPrivateSignal);
    void portChanged(lmrs::core::accessory::rbus::PortIndex port, QPrivateSignal);

protected:
    void setControl(DetectorControl *newControl) override;

private:
    void onDetectorInfoChanged(accessory::DetectorInfo info);

    std::optional<accessory::rbus::ModuleId> m_module;
    std::optional<accessory::rbus::PortIndex> m_port;
};

// =====================================================================================================================

class AutomationTypeModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum DataRole {
        NameRole = Qt::DisplayRole,
        TypeRole = Qt::UserRole,
        ItemRole,
        EventRole,
        ActionRole,
    };

    Q_ENUM(DataRole)

    explicit AutomationTypeModel(QObject *parent = nullptr);

    template<ItemType T>
    void registerType()
    {
        registerType(qRegisterMetaType<T *>(), [](QObject *parent) { return new T{parent}; });
    }

    template<ItemType T = Item>
    T *fromType(QMetaType type, QObject *parent = nullptr) const
    {
        return dynamic_cast<T *>(fromMetaType(type, parent));
    }

    template<ItemType T = Item, ForwardDeclared<QJsonObject> JsonObject>
    T *fromJsonObject(JsonObject object, QObject *parent = nullptr) const
    {
        return dynamic_cast<T *>(fromJsonObject(QMetaType::fromType<T *>(), std::move(object), parent));
    }

    template<ItemType T = Item>
    T *fromJson(QByteArray json, QObject *parent = nullptr) const
    {
        return dynamic_cast<T *>(fromJson(QMetaType::fromType<T *>(), std::move(json), parent));
    }

    template<ItemType T = Item>
    T *fromPrototype(const T *prototype, QObject *parent = nullptr) const
    {
        return dynamic_cast<T *>(fromMetaObject(prototype->metaObject(), parent));
    }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    const Action *actionItem(const QModelIndex &index) const;
    const Event *eventItem(const QModelIndex &index) const;
    const Item *item(const QModelIndex &index) const;
    QMetaType itemType(const QModelIndex &index) const;

private:
    using Factory = std::function<Item *(QObject *)>;
    void registerType(int typeId, Factory createEvent);

    static bool verifyProperty(const Item *target, const Parameter &parameter, QByteArrayView propertyName);

    Item *fromMetaType(QMetaType type, QObject *parent) const;
    Item *fromMetaObject(const QMetaObject *metaObject, QObject *parent) const;
    Item *fromJsonObject(QMetaType baseType, QJsonObject object, QObject *parent) const;
    Item *fromJson(QMetaType baseType, QByteArray json, QObject *parent) const;

    QHash<int, Factory> m_factories;

    class Row
    {
    public:
        using ItemPointer = std::shared_ptr<Item>;

        Row() = default;

        explicit Row(l10n::String text);
        explicit Row(ItemPointer event);

        Action *action() const { return dynamic_cast<Action *>(item()); }
        Event *event() const { return dynamic_cast<Event *>(item()); }
        Item *item() const;

        std::optional<l10n::String> text() const;

    private:
        std::variant<l10n::String, ItemPointer> m_value;
    };

    QList<Row> m_rows;
};

// =====================================================================================================================

class AutomationModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum DataRole {
        NameRole = Qt::DisplayRole,
        EventRole = Qt::UserRole,
    };

    explicit AutomationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const Event *eventItem(int row) const { return eventItem(index(row, 0)); }
    const Event *eventItem(const QModelIndex &index) const;

    int appendEvent(Event *event);
    int insertEvent(Event *event, int before);

    int appendAction(const QModelIndex &index, Action *action);
    int insertAction(const QModelIndex &index, Action *action, int before);

    bool removeRows(int row, int count, const QModelIndex &parent = {}) override;

    void setDevice(core::Device *newDevice);
    core::Device *device() const;

public slots:
    void clear();

signals:
    void actionChanged(lmrs::core::automation::Action *action);
    void eventChanged(lmrs::core::automation::Event *event);

protected:
    Event *eventItem(const QModelIndex &index);
    void observeItem(Item *item, const QMetaObject *baseType, QMetaMethod observer);

protected slots:
    void onActionChanged();
    void onEventChanged();

private:
    class Private;
    Private *const d;
};

// =====================================================================================================================

using AutomationModelReader = FileFormatReader<AutomationModel, const AutomationTypeModel *>;
using AutomationModelWriter = FileFormatWriter<AutomationModel>;

// =====================================================================================================================

/*
class SignalAction;
class PowerControlAction;
class AudioAction;
class CameraAction;
*/

} // namespace lmrs::core::automation

#endif // LMRS_CORE_AUTOMATIONMODEL_H
