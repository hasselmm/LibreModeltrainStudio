#include <lmrs/core/algorithms.h>
#include <lmrs/core/automationmodel.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/staticinit.h>

#include <QtTest>

namespace lmrs::core::automation::tests {

Q_NAMESPACE

enum class Flavour {
    Initial,
    Modified,
    // FIXME: Bound,
};

Q_ENUM_NS(Flavour)

namespace {

// FIXME: maybe move to algorithms
template<typename T>
bool contains(const QList<T> &list, const QList<T> &sublist)
{
    const auto subset = QSet{sublist};
    const auto intersection = QSet{list}.intersect(subset);
    return intersection.size() == subset.size();
}

template<class T, Flavour expectedFlavour, class BaseType>
T *flavour_cast(BaseType *object, Flavour flavour)
{
    if (flavour == expectedFlavour)
        return core::checked_cast<T *>(object);

    return nullptr;
}

std::unique_ptr<Event> flavouredEvent(std::unique_ptr<Event> event, Flavour flavour)
{
    if (dynamic_cast<CanDetectorEvent *>(event.get())) {
        if (const auto modifiedEvent = flavour_cast<CanDetectorEvent, Flavour::Modified>(event.get(), flavour)) {
            modifiedEvent->setNetwork(0x310b);
            modifiedEvent->setModule(1);
            modifiedEvent->setPort(2);
            modifiedEvent->setType(CanDetectorEvent::Type::Entering);
        }

        return event;
    }

    if (dynamic_cast<RBusDetectorEvent *>(event.get())) {
        if (const auto modifiedEvent = flavour_cast<RBusDetectorEvent, Flavour::Modified>(event.get(), flavour)) {
            modifiedEvent->setModule(1);
            modifiedEvent->setPort(2);
            modifiedEvent->setType(CanDetectorEvent::Type::Entering);
        }

        return event;
    }

    if (dynamic_cast<RBusDetectorGroupEvent *>(event.get())) {
        if (const auto modifiedEvent = flavour_cast<RBusDetectorGroupEvent, Flavour::Modified>(event.get(), flavour)) {
            modifiedEvent->setGroup(1);
            modifiedEvent->setType(CanDetectorEvent::Type::Entering);
        }

        return event;
    }

    if (dynamic_cast<TurnoutEvent *>(event.get())) {
        if (const auto modifiedEvent = flavour_cast<TurnoutEvent, Flavour::Modified>(event.get(), flavour)) {
            modifiedEvent->setPrimaryAddress(1);
            modifiedEvent->setPrimaryState(dcc::TurnoutState::Straight);
            modifiedEvent->setSecondaryAddress(2);
            modifiedEvent->setSecondaryState(dcc::TurnoutState::Branched);
        }

        return event;
    }

    LMRS_UNIMPLEMENTED_FOR_METATYPE(event->metaObject()->metaType());
    return nullptr;
}

std::unique_ptr<Action> flavouredAction(std::unique_ptr<Action> action, Flavour flavour)
{
    if (dynamic_cast<MessageAction *>(action.get())) {
        if (const auto modifiedAction = flavour_cast<MessageAction, Flavour::Modified>(action.get(), flavour)) {
            modifiedAction->setMessage("Turnout {address} has switched to state {state}"_L1);
        }

        return action;
    }

    if (dynamic_cast<TurnoutAction *>(action.get())) {
        if (const auto modifiedAction = flavour_cast<TurnoutAction, Flavour::Modified>(action.get(), flavour)) {
            modifiedAction->setAddress(1);
            modifiedAction->setState(dcc::TurnoutState::Red);
        }

        return action;
    }

    if (dynamic_cast<VehicleAction *>(action.get())) {
        if (const auto modifiedAction = flavour_cast<VehicleAction, Flavour::Modified>(action.get(), flavour)) {
            modifiedAction->setAddress(1);
            modifiedAction->setDirection(dcc::Direction::Forward);
            modifiedAction->setSpeed(2);
        }

        return action;
    }

    LMRS_UNIMPLEMENTED_FOR_METATYPE(action->metaObject()->metaType());
    return nullptr;
}

std::unique_ptr<Event> makeEventForActionType(QMetaType metaType, QObject *parent)
{
    if (metaType == QMetaType::fromType<MessageAction *>())
        return flavouredEvent(std::make_unique<TurnoutEvent>(parent), Flavour::Initial);
    if (metaType == QMetaType::fromType<TurnoutAction *>())
        return flavouredEvent(std::make_unique<TurnoutEvent>(parent), Flavour::Initial);
    if (metaType == QMetaType::fromType<VehicleAction *>())
        return flavouredEvent(std::make_unique<CanDetectorEvent>(parent), Flavour::Initial);

    LMRS_UNIMPLEMENTED_FOR_METATYPE(metaType);
    return {};
}

QJsonObject addAction(QJsonObject event, QJsonObject action)
{
    event.insert("$actions"_L1, QJsonArray{std::move(action)});
    return event;
}

QJsonObject jsonForItemType(QMetaType metaType, Flavour flavour)
{
    if (metaType == QMetaType::fromType<CanDetectorEvent *>()) {
        return {
            {"$schema"_L1,          "https://taschenorakel.de/lmrs/core/automation/CanDetectorEvent"_L1},
            {"network"_L1,          flavour == Flavour::Initial ? QJsonValue{} : 0x310b},
            {"module"_L1,           flavour == Flavour::Initial ? QJsonValue{} : 1},
            {"port"_L1,             flavour == Flavour::Initial ? QJsonValue{} : 2},
            {"type"_L1,             flavour == Flavour::Initial ? "Any"_L1 : "Entering"_L1},
            {"$actions"_L1,         QJsonArray{}},
        };
    }

    if (metaType == QMetaType::fromType<RBusDetectorEvent *>()) {
        return {
            {"$schema"_L1,          "https://taschenorakel.de/lmrs/core/automation/RBusDetectorEvent"_L1},
            {"module"_L1,           flavour == Flavour::Initial ? QJsonValue{} : 1},
            {"port"_L1,             flavour == Flavour::Initial ? QJsonValue{} : 2},
            {"type"_L1,             flavour == Flavour::Initial ? "Any"_L1 : "Entering"_L1},
            {"$actions"_L1,         QJsonArray{}},
        };
    }

    if (metaType == QMetaType::fromType<RBusDetectorGroupEvent *>()) {
        return {
            {"$schema"_L1,          "https://taschenorakel.de/lmrs/core/automation/RBusDetectorGroupEvent"_L1},
            {"group"_L1,            flavour == Flavour::Initial ? QJsonValue{} : 1},
            {"type"_L1,             flavour == Flavour::Initial ? "Any"_L1 : "Entering"_L1},
            {"$actions"_L1,         QJsonArray{}},
        };
    }

    if (metaType == QMetaType::fromType<TurnoutEvent *>()) {
        return {
            {"$schema"_L1,          "https://taschenorakel.de/lmrs/core/automation/TurnoutEvent"_L1},
            {"primaryAddress"_L1,   flavour == Flavour::Initial ? QJsonValue{} : 1},
            {"primaryState"_L1,     flavour == Flavour::Initial ? QJsonValue{} : "Straight"_L1},
            {"secondaryAddress"_L1, flavour == Flavour::Initial ? QJsonValue{} : 2},
            {"secondaryState"_L1,   flavour == Flavour::Initial ? QJsonValue{} : "Branched"_L1},
            {"$actions"_L1,         QJsonArray{}},
        };
    }

    if (metaType == QMetaType::fromType<MessageAction *>()) {
        return addAction(jsonForItemType(QMetaType::fromType<TurnoutEvent *>(), Flavour::Initial), {
            {"$schema"_L1,          "https://taschenorakel.de/lmrs/core/automation/MessageAction"_L1},
            {"message"_L1,          flavour == Flavour::Initial ? ""_L1 : "Turnout {address} has switched to state {state}"_L1},
        });
    }

    if (metaType == QMetaType::fromType<TurnoutAction *>()) {
        return addAction(jsonForItemType(QMetaType::fromType<TurnoutEvent *>(), Flavour::Initial), {
            {"$schema"_L1,          "https://taschenorakel.de/lmrs/core/automation/TurnoutAction"_L1},
            {"address"_L1,          flavour == Flavour::Initial ? QJsonValue{} : 1},
            {"state"_L1,            flavour == Flavour::Initial ? QJsonValue{} : "Branched"_L1}, // FIXME: decide how to deal with Branched/Straight vs. Red/Green
        });
    }

    if (metaType == QMetaType::fromType<VehicleAction *>()) {
        return addAction(jsonForItemType(QMetaType::fromType<CanDetectorEvent *>(), Flavour::Initial), {
            {"$schema"_L1,          "https://taschenorakel.de/lmrs/core/automation/VehicleAction"_L1},
            {"address"_L1,          flavour == Flavour::Initial ? QJsonValue{} : 1},
            {"direction"_L1,        flavour == Flavour::Initial ? QJsonValue{} : "Forward"_L1},
            {"speed"_L1,            flavour == Flavour::Initial ? QJsonValue{} : 2},
        });
    }

    LMRS_UNIMPLEMENTED_FOR_METATYPE(metaType);
    return {};
}

FileFormat findReaderFormat(const FileFormat &writerFormat)
{
    for (const auto &readerFormat: AutomationModelReader::fileFormats()) {
        if (readerFormat.mimeType != writerFormat.mimeType)
            continue;

        if (!contains(readerFormat.extensions, writerFormat.extensions))
            continue;

        return readerFormat;
    }

    return {};
}

enum RestFlag { Report, Reset };
int logMessageCount(RestFlag reset = Report)
{
    static auto s_logMessageCount = 0;
    static const QtMessageHandler s_previous = qInstallMessageHandler([](auto type, const auto &ctx, const auto &msg) {
        s_previous(type, ctx, msg);
        ++s_logMessageCount;
    });

    if (reset == Reset)
        s_logMessageCount = 0;

    return s_logMessageCount;
}

} // namespace

class AutomationTest : public logging::StaticInitTesting<QObject>
{
    Q_OBJECT

public:
    using StaticInitTesting::StaticInitTesting;

private slots:
    void testSerializationRoundtrip_data()
    {
        QTest::addColumn<FileFormat>("writerFormat");
        QTest::addColumn<Event *>("event");
        QTest::addColumn<QJsonObject>("expectedJson");

        const auto types = std::make_unique<AutomationTypeModel>();

        for (const auto &writerFormat: AutomationModelWriter::fileFormats()) {
            for (const auto &index: types.get()) {
                const auto itemType = types->itemType(index);

                if (!itemType.isValid())
                    continue; // skip group separators

                auto previousJson = QList<QJsonObject>{};

                for (const auto &entry: QMetaTypeId<Flavour>()) {
                    auto tag = writerFormat.mimeType.toUtf8()
                            + '/' + QByteArray{itemType.metaObject()->className()}
                            + '/' + QByteArray{entry.key()};

                    auto event = std::unique_ptr<Event>{};
                    const auto flavour = entry.value();

                    if (itemType.metaObject()->inherits(&Action::staticMetaObject)) {
                        // actions need a wrapping event

                        event = makeEventForActionType(itemType, this);
                        QVERIFY2(event != nullptr, tag.constData());

                        auto action = types->fromType<Action>(itemType, this);
                        action = flavouredAction(std::move(action), flavour);
                        const auto actionIndex = event->appendAction(action.release());

                        QCOMPARE(event->actionCount(), 1);
                        QCOMPARE(actionIndex, 0);
                    } else {
                        // events can be created directly

                        event = types->fromType<Event>(itemType, this);
                        QVERIFY2(event != nullptr, tag.constData());

                        event = flavouredEvent(std::move(event), flavour);

                        QVERIFY2(event != nullptr, tag.constData());
                    }

                    // final checks on the event, and then store it

                    QVERIFY2(event != nullptr, tag.constData());
                    QVERIFY2(event->parent() == this, tag.constData());

                    auto expectedJson = jsonForItemType(itemType, flavour);

                    QTest::addRow("%s", tag.constData())
                            << writerFormat << event.release() << expectedJson;

                    QVERIFY2(!previousJson.contains(expectedJson)
                             || expectedJson.isEmpty(), tag.constData());

                    previousJson.emplaceBack(std::move(expectedJson));
                }
            }
        }
    }

    void testSerializationRoundtrip()
    {
        logMessageCount(Reset);

        QFETCH(const FileFormat, writerFormat);
        QFETCH(Event *const, event);
        QFETCH(const QJsonObject, expectedJson);

        // pre-validate the object unter test

        QVERIFY(event != nullptr);
        QCOMPARE(event->parent(), this);
        QVERIFY(!expectedJson.isEmpty());
        QCOMPARE(event->toJsonObject(), expectedJson);

        // validate the requested file format

        const auto readerFormat = findReaderFormat(writerFormat);

        QVERIFY(writerFormat.isValid());
        QVERIFY(readerFormat.isValid());

        const auto writerFactory = AutomationModelWriter::fromFileFormat(writerFormat);

        QVERIFY(writerFactory.isValid());
        QVERIFY(writerFactory.fromFileName);
        QVERIFY(writerFactory.fromDevice);

        const auto readerFactory = AutomationModelReader::fromFileFormat(writerFormat);

        QVERIFY(readerFactory.isValid());
        QVERIFY(readerFactory.fromFileName);
        QVERIFY(readerFactory.fromDevice);

        // test writing

        const auto device = std::make_unique<QBuffer>();
        const auto writer = writerFactory.fromDevice(device.get());

        QVERIFY(writer);

        const auto modelToWrite = std::make_unique<AutomationModel>();
        const auto index = modelToWrite->appendEvent(event);

        QCOMPARE(modelToWrite->rowCount(), 1);
        QCOMPARE(modelToWrite->eventItem(index), event);
        QCOMPARE(logMessageCount(), 0);

        const auto writingSucceeded = writer->write(modelToWrite.get());

        QVERIFY(writingSucceeded);
        QVERIFY(!device->data().isEmpty());
        QVERIFY(!device->isOpen());
        QCOMPARE(logMessageCount(), 0);

        // test reading

        const auto reader = readerFactory.fromDevice(device.get());

        QVERIFY(reader);

        const auto types = std::make_unique<AutomationTypeModel>();
        const auto restoredModel = reader->read(types.get());

        QVERIFY(restoredModel != nullptr);
        QCOMPARE(restoredModel->rowCount(), 1);

        const auto restoredEvent = restoredModel->eventItem(0);

        QVERIFY(restoredEvent != nullptr);
        QCOMPARE(restoredEvent->toJsonObject(), expectedJson);
        QVERIFY(!device->isOpen());
        QCOMPARE(logMessageCount(), 0);
    }
};

} // namespace lmrs::core::automation::tests

QTEST_MAIN(lmrs::core::automation::tests::AutomationTest)

#include "tst_automation.moc"
