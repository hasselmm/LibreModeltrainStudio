#include <lmrs/core/logging.h>
#include <lmrs/core/userliterals.h>
#include <lmrs/roco/z21client.h>

#include <QNetworkDatagram>
#include <QUdpSocket>

#include <QtTest>

template<typename T> QList(int, const T &) -> QList<T>;

namespace lmrs::roco::z21::tests {

using namespace std::chrono_literals;
using std::chrono::milliseconds;

struct MockMessage
{
    QByteArray prefix;
    int suffixLength;

    QByteArray response;
};

const auto s_prefix_queryDetectorInfo_canAny        = "07 00 | c4 00 | 00 | d0 00"_hex;
const auto s_prefix_queryDetectorInfo_canOne        = "07 00 | c4 00 | 00 | 12 34"_hex;
const auto s_prefix_queryDetectorInfo_canTwo        = "07 00 | c4 00 | 00 | 56 78"_hex;
const auto s_prefix_queryDetectorInfo_lissy         = "07 00 | a4 00 | 82 | 00 17"_hex;
const auto s_prefix_queryDetectorInfo_loconet_rm    = "07 00 | a4 00 | 81 | 03 f8"_hex;
const auto s_prefix_queryDetectorInfo_loconet_sic   = "07 00 | a4 00 | 80 | 00 00"_hex;
const auto s_prefix_queryDetectorInfo_rbus          = "05 00 | 81 00 | 01"_hex;
const auto s_prefix_queryTrackStatus                = "07 00 | 40 00 | 21 24 | 00"_hex;
const auto s_prefix_subscribe                       = "08 00 | 50 00"_hex;

const auto s_response_detectorInfo_canOne           = "0e 00 | c4 00 | 34 12 | 00 01 | 00 | 01 | 00 01 | 00 00"
                                                      "0e 00 | c4 00 | 34 12 | 00 01 | 01 | 01 | 00 11 | 00 00"
                                                      "0e 00 | c4 00 | 34 12 | 00 01 | 01 | 11 | ea 86 | e8 c8"
                                                      "0e 00 | c4 00 | 34 12 | 00 01 | 01 | 12 | 1a 09 | 00 00"_hex;
const auto s_response_detectorInfo_canTwo           = "0e 00 | c4 00 | 78 56 | 01 01 | 00 | 01 | 01 12 | 00 00"
                                                      "0e 00 | c4 00 | 78 56 | 01 01 | 00 | 11 | 20 03 | 00 00"_hex;
const auto s_response_queryDetectorInfo_loconet_sic = "08 00 | a4 00 | 01 | 00 40 | 00"
                                                      "08 00 | a4 00 | 01 | 00 41 | 01"_hex;
const auto s_response_detectorInfo_rbus             = "0f 00 | 80 00 | 01 | 01 02 04 08 10 20 40 80 11 22"_hex;
const auto s_response_trackStatus_powerOn           = "08 00 | 40 00 | 62 22 | 00 | 08"_hex;

template<typename T>
auto iota(int count)
{
    auto sequence = QList<T>{};
    sequence.reserve(count);

    for (auto i = 0; i < count; ++i)
        sequence += i;

    return sequence;
}

template<typename T>
struct Patch
{
    int offset;
    T value;
};

template<typename T>
Patch(int, T) -> Patch<T>;

template<typename T>
QList<T> operator|(QList<T> &&list, const Patch<T> &patch)
{
    list[patch.offset] = patch.value;
    return list;
}

template<typename T>
QList<T> flatten(QList<QVariantList> input)
{
    auto result = QList<T>{};
    result.reserve(input.size());

    for (const auto &list: input) {
        for (const auto &entry: list) {
            if (entry.typeId() == qMetaTypeId<T>())
                result += qvariant_cast<T>(entry);
            else if (entry.typeId() == qMetaTypeId<QList<T>>())
                result += qvariant_cast<QList<T>>(entry);
            else
                Q_UNREACHABLE();
        }
    }

    return result;
}

template<typename T>
QList<T> flatten(QList<QList<T>> input)
{
    auto result = QList<T>{};
    result.reserve(input.size());

    for (const auto &list: input)
        result += list;

    return result;
}

class FakeSocket : public QUdpSocket
{
    Q_OBJECT

public:
    explicit FakeSocket(QList<MockMessage> mockup, QObject *parent = nullptr)
        : QUdpSocket{parent}
        , m_mockMessages{std::move(mockup)}
    {
        connect(this, &FakeSocket::readyRead, this, &FakeSocket::onReadyRead);
    }

signals:
    void messageReceived(QByteArray message);

private:
    void onReadyRead()
    {
        while (bytesAvailable() > 0) {
            const auto datagram = receiveDatagram();
            auto buffer = datagram.data();

            qCDebug(core::logger(this), "received: %s", buffer.toHex(' ').constData());

            while (!buffer.isEmpty()) {
                auto message = QByteArray{};

                for (const auto &mock: m_mockMessages) {
                    const auto messageLength = mock.prefix.length() + mock.suffixLength;

                    if (buffer.length() < messageLength)
                        continue;
                    if (!buffer.startsWith(mock.prefix))
                        continue;

                    message = buffer.left(messageLength);
                    qCDebug(core::logger(this), "mock message matched: %s", message.toHex(' ').constData());

                    if (!mock.response.isEmpty()) {
                        qCDebug(core::logger(this), "sending: %s", mock.response.toHex(' ').constData());

                        writeDatagram(mock.response, datagram.senderAddress(),
                                      static_cast<quint16>(datagram.senderPort()));
                    }

                    buffer = buffer.mid(messageLength);
                    break;
                }

                if (message.isEmpty()) {
                    qCWarning(core::logger(this), "Unexpected message received: %s", buffer.toHex(' ').constData());
                    break;
                }

                emit messageReceived(std::move(message));
            }
        }
    }

    QList<MockMessage> m_mockMessages;
};

class ClientTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private:
    std::unique_ptr<QUdpSocket> createFakeSocket(QList<MockMessage> mockMessages)
    {
        mockMessages += {
            {s_prefix_subscribe, 4, {}},
            {s_prefix_queryTrackStatus, 0, s_response_trackStatus_powerOn},
            {s_prefix_queryDetectorInfo_rbus, 0, s_response_detectorInfo_rbus},
            {s_prefix_queryDetectorInfo_loconet_sic, 0, s_response_queryDetectorInfo_loconet_sic},
            {s_prefix_queryDetectorInfo_loconet_rm, 0, {}},
            {s_prefix_queryDetectorInfo_lissy, 0, {}},
            {s_prefix_queryDetectorInfo_canAny, 0, s_response_detectorInfo_canOne + s_response_detectorInfo_canTwo},
            {s_prefix_queryDetectorInfo_canOne, 0, s_response_detectorInfo_canOne},
            {s_prefix_queryDetectorInfo_canTwo, 0, s_response_detectorInfo_canTwo},
        };

        auto socket = std::make_unique<FakeSocket>(std::move(mockMessages));

        if (!socket->bind(QHostAddress::LocalHost, 0, QUdpSocket::DontShareAddress | QUdpSocket::ReuseAddressHint)) {
            qCWarning(core::logger(this), "Could not bind fake socket: %ls", qUtf16Printable(socket->errorString()));
            return {};
        }

        return socket;
    }

    std::unique_ptr<Client> createMockClient(QList<MockMessage> mockMessages = {})
    {
        auto client = std::make_unique<Client>();
        auto socket = createFakeSocket(std::move(mockMessages));
        auto connected = QSignalSpy{client.get(), &Client::connected};

        client->connectToHost({}, socket->localAddress(), socket->localPort());

        if (!connected.wait(milliseconds{1s}.count())) {
            qCWarning(core::logger(this), "Connecting to fake socket has failed");
            return {};
        }

        socket.release()->setParent(client.get());

        return client;
    }

private slots:
    void testConnect()
    {
        auto client = createMockClient();

        QVERIFY(client);
        QVERIFY(client->isConnected());
        QCOMPARE(client->trackStatus(), Client::TrackStatus::PowerOn);
    }

    void testQueryDetectorInfo_data()
    {
        using dcc::Direction;
        using dcc::VehicleAddress;
        using rm::DetectorAddress;

        using Occupancy = core::DetectorInfo::Occupancy;
        using PowerState = core::DetectorInfo::PowerState;

        QTest::addColumn<DetectorAddress>("genericAddress");
        QTest::addColumn<QList<DetectorAddress>>("expectedModules");
        QTest::addColumn<QList<int>>("expectedPorts");
        QTest::addColumn<QList<Occupancy>>("expectedOccupancies");
        QTest::addColumn<QList<PowerState>>("expectedPowerStates");
        QTest::addColumn<QList<QList<VehicleAddress>>>("expectedVehicles");
        QTest::addColumn<QList<QList<Direction>>>("expectedDirections");

        QTest::newRow("rbus:group")
                << rm::DetectorAddress::forRBusGroup(1)
                << QList{rm::rbus::PortsPerGroup, rm::DetectorAddress::forRBusGroup(1)}
                << iota<int>(rm::rbus::PortsPerGroup)
                << (QList{rm::rbus::PortsPerGroup, Occupancy::Free}
                    | Patch{0, Occupancy::Occupied}
                    | Patch{9, Occupancy::Occupied}
                    | Patch{18, Occupancy::Occupied}
                    | Patch{27, Occupancy::Occupied}
                    | Patch{36, Occupancy::Occupied}
                    | Patch{45, Occupancy::Occupied}
                    | Patch{54, Occupancy::Occupied}
                    | Patch{63, Occupancy::Occupied}
                    | Patch{64, Occupancy::Occupied}
                    | Patch{68, Occupancy::Occupied}
                    | Patch{73, Occupancy::Occupied}
                    | Patch{77, Occupancy::Occupied})
                << QList{rm::rbus::PortsPerGroup, PowerState::Unknown}
                << QList{rm::rbus::PortsPerGroup, QList<VehicleAddress>{}}
                << QList{rm::rbus::PortsPerGroup, QList<Direction>{}};

        QTest::newRow("loconet:SIC")
                << DetectorAddress::forLoconetSIC()
                << QList{2, DetectorAddress::forLoconetModule(1)}
                << QList{0, 1, }
                << QList{Occupancy::Free, Occupancy::Occupied}
                << QList{PowerState::Unknown, PowerState::Unknown}
                << QList{QList<VehicleAddress>{}, {}}
                << QList{QList<Direction>{}, {}};

        QTest::newRow("loconet:report-module")
                << DetectorAddress::forLoconetModule(1)
                << QList{2, DetectorAddress::forLoconetModule(1)}
                << QList{0, 1, }
                << QList{Occupancy::Free, Occupancy::Occupied}
                << QList{PowerState::Unknown, PowerState::Unknown}
                << QList{QList<VehicleAddress>{}, {}}
                << QList{QList<Direction>{}, {}};

        QTest::newRow("loconet:lissy-module")
                << DetectorAddress::forLissyModule(23)
                << QList{2, DetectorAddress::forLissyModule(23)}
                << QList{0, 1, }
                << QList{Occupancy::Free, Occupancy::Occupied}
                << QList{PowerState::Unknown, PowerState::Unknown}
                << QList{QList<VehicleAddress>{}, {}}
                << QList{QList<Direction>{}, {}};

        const auto canModuleOne = DetectorAddress::forCanModule(0x1234, 256);
        const auto canModuleTwo = DetectorAddress::forCanModule(0x5678, 257);

        QTest::newRow("can:all")
                << DetectorAddress::forCanNetwork(rm::can::NetworkIdAny)
                << QList{canModuleOne, canModuleOne, canModuleTwo}
                << QList{0, 1, 0, }
                << QList{Occupancy::Free, Occupancy::Occupied, Occupancy::Occupied}
                << QList{PowerState::On, PowerState::On, PowerState::Overload}
                << QList{QList<VehicleAddress>{}, {1770, 2280, 2330}, {800}}
                << QList{QList<Direction>{}, {Direction::Forward, Direction::Reverse, Direction::Unknown}, {Direction::Unknown}};

        QTest::newRow("can:one")
                << DetectorAddress::forCanNetwork(0x1234)
                << QList{canModuleOne, canModuleOne}
                << QList{0, 1, }
                << QList{Occupancy::Free, Occupancy::Occupied}
                << QList{PowerState::On, PowerState::On}
                << QList{QList<VehicleAddress>{}, {1770, 2280, 2330}}
                << QList{QList<Direction>{}, {Direction::Forward, Direction::Reverse, Direction::Unknown}};

        QTest::newRow("can:two")
                << DetectorAddress::forCanNetwork(0x5678)
                << QList{canModuleTwo}
                << QList{0}
                << QList{Occupancy::Occupied}
                << QList{PowerState::Overload}
                << QList{{QList<VehicleAddress>{800}, }}
                << QList{{QList<Direction>{Direction::Unknown}, }};
    }

    void testQueryDetectorInfo()
    {
        QFETCH(rm::DetectorAddress,                     genericAddress);
        QFETCH(QList<rm::DetectorAddress>,              expectedModules);
        QFETCH(QList<int>,                              expectedPorts);
        QFETCH(QList<core::DetectorInfo::Occupancy>,    expectedOccupancies);
        QFETCH(QList<core::DetectorInfo::PowerState>,   expectedPowerStates);
        QFETCH(QList<QList<dcc::VehicleAddress>>,       expectedVehicles);
        QFETCH(QList<QList<dcc::Direction>>,            expectedDirections);

        // sanity check the expected information
        QCOMPARE(expectedPorts.count(),         expectedModules.count());
        QCOMPARE(expectedOccupancies.count(),   expectedModules.count());
        QCOMPARE(expectedPowerStates.count(),   expectedModules.count());
        QCOMPARE(expectedVehicles.count(),      expectedModules.count());
        QCOMPARE(expectedDirections.count(),    expectedModules.count());

        // create a mocked Z21 client
        auto client = createMockClient();

        QVERIFY(client);
        QVERIFY(client->isConnected());

        auto canDetectorInfoReceived = QSignalSpy{client.get(), &Client::canDetectorInfoReceived};
        auto loconetDetectorInfoReceived = QSignalSpy{client.get(), &Client::loconetDetectorInfoReceived};
        auto rbusDetectorInfoReceived = QSignalSpy{client.get(), &Client::rbusDetectorInfoReceived};
        auto detectorInfoReceived = QSignalSpy{client.get(), &Client::detectorInfoReceived};

        auto actualCanInfo = QList<QList<CanDetectorInfo>>{};
        auto actualLoconetInfo = QList<QList<LoconetDetectorInfo>>{};
        auto actualRBusInfo = QList<RBusDetectorInfo>{};
        auto actualDetectorInfo = QList<core::DetectorInfo>{};

        // run native queries
        if (genericAddress.type() == rm::DetectorAddress::Type::CanNetwork) {
            client->queryCanDetectorInfo(genericAddress.canNetwork(), [&actualDetectorInfo, &actualCanInfo](auto info) {
                actualDetectorInfo += CanDetectorInfo::merge(info);
                actualCanInfo += std::move(info);
            });

            QVERIFY(QTest::qWaitFor([&actualCanInfo] {
                return !actualCanInfo.isEmpty();
            }, milliseconds(1s).count()));
        } else if (genericAddress.type() == rm::DetectorAddress::Type::RBusModule) {
            client->queryRBusDetectorInfo(genericAddress.rbusGroup(), [&actualDetectorInfo, &actualRBusInfo](auto info) {
                actualDetectorInfo += info;
                actualRBusInfo += std::move(info);
            });

            QVERIFY(QTest::qWaitFor([&actualRBusInfo] {
                return !actualRBusInfo.isEmpty();
            }, milliseconds(1s).count()));

            auto expectedOccupancyState = QBitArray{expectedOccupancies.size()};

            for (auto i = 0; i < expectedOccupancies.size(); ++i)
                expectedOccupancyState[i] = (expectedOccupancies[i] == core::DetectorInfo::Occupancy::Occupied);

            QCOMPARE(actualRBusInfo.count(), 1);

            QVERIFY(actualRBusInfo[0].isValid());
            QCOMPARE(actualRBusInfo[0].group(), expectedModules[0].rbusGroup());
            QCOMPARE(actualRBusInfo[0].occupancy(), expectedOccupancyState);
        } else if (const auto [query, address] = LoconetDetectorInfo::address(genericAddress);
                   query != LoconetDetectorInfo::Query::Invalid) {
            client->queryLoconetDetectorInfo(query, address, [&actualDetectorInfo, &actualLoconetInfo](auto info) {
                actualDetectorInfo += LoconetDetectorInfo::merge(info);
                actualLoconetInfo += std::move(info);
            });

            QSKIP("So far we totally fail to merge and emit Loconet messages");

            QVERIFY(QTest::qWaitFor([&actualLoconetInfo] {
                return !actualLoconetInfo.isEmpty();
            }, milliseconds(1s).count()));
        } else {
            QFAIL("Unexpected address type");
        }

        // verify the correct number of signals is emitted
        QCOMPARE(flatten<CanDetectorInfo>(canDetectorInfoReceived), flatten(actualCanInfo));
        QCOMPARE(flatten<LoconetDetectorInfo>(loconetDetectorInfoReceived), flatten(actualLoconetInfo));
        QCOMPARE(flatten<RBusDetectorInfo>(rbusDetectorInfoReceived), actualRBusInfo);
        QCOMPARE(flatten<core::DetectorInfo>(detectorInfoReceived), actualDetectorInfo);

        // verify we received sufficient generic detector info
        QCOMPARE(actualDetectorInfo.count(), expectedModules.count());

        // verify we received the expected generic detector info
        for (auto i = 0; i < expectedModules.count(); ++i) {
            QCOMPARE(std::make_tuple(i, actualDetectorInfo[i].address()),
                     std::make_tuple(i, expectedModules[i]));
// FIXME: adopt of port inforation having moved in the address
//            QCOMPARE(std::make_tuple(i, actualDetectorInfo[i].port()),
//                     std::make_tuple(i, expectedPorts[i]));
            QCOMPARE(std::make_tuple(i, actualDetectorInfo[i].occupancy()),
                     std::make_tuple(i, expectedOccupancies[i]));
            QCOMPARE(std::make_tuple(i, actualDetectorInfo[i].powerState()),
                     std::make_tuple(i, expectedPowerStates[i]));
            QCOMPARE(std::make_tuple(i, actualDetectorInfo[i].vehicles()),
                     std::make_tuple(i, expectedVehicles[i]));
            QCOMPARE(std::make_tuple(i, actualDetectorInfo[i].directions()),
                     std::make_tuple(i, expectedDirections[i]));
        }
    }
};

} // namespace lmrs::roco::z21::tests

QTEST_MAIN(lmrs::roco::z21::tests::ClientTest)

#include "tst_z21client.moc"
