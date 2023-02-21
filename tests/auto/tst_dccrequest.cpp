#include <lmrs/core/dccrequest.h>
#include <lmrs/core/userliterals.h>

#include <QtTest>

namespace lmrs::core::dcc::tests {

class RequestTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testParsing_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<bool>("expectExtendedAddress");
        QTest::addColumn<int>("expectedAddress");

        // TODO: test command and arguments

        QTest::newRow("setSpeed28:basic") << "03 68 6B"_hex << false << 3;
        QTest::newRow("setSpeed28:extended") << "C3 3E 78 85"_hex << true << 830;
    }

    void testParsing()
    {
        const QFETCH(QByteArray, data);
        const QFETCH(bool, expectExtendedAddress);
        const QFETCH(int, expectedAddress);

        const auto request = Request{data};

        QCOMPARE(request.toByteArray().toHex(' '), data.toHex(' '));
        QCOMPARE(request.hasExtendedAddress(), expectExtendedAddress);
        QCOMPARE(request.address(), expectedAddress);
    }

    void testRequestGenerator_data()
    {
        QTest::addColumn<Request>("request");
        QTest::addColumn<QByteArray>("expectedBytes");

        QTest::newRow("reset")
                << dcc::Request::reset()
                << "00 00 00"_hex;

        QTest::newRow("setSpeed14:basic")
                << dcc::Request::setSpeed14(65, 4, dcc::Direction::Forward, true)
                << "41 74 35"_hex;
        QTest::newRow("setSpeed14:extended")
                << dcc::Request::setSpeed14(650, 5, dcc::Direction::Reverse, false)
                << "C2 8A 45 0D"_hex;

        QTest::newRow("setSpeed28:basic")
                << dcc::Request::setSpeed28(3, 16, dcc::Direction::Forward)
                << "03 68 6B"_hex;
        QTest::newRow("setSpeed28:extended")
                << dcc::Request::setSpeed28(830, 17, dcc::Direction::Reverse)
                << "C3 3E 58 A5"_hex;

        QTest::newRow("setSpeed128:basic")
                << dcc::Request::setSpeed126(93, 23, dcc::Direction::Forward)
                << "5D 3F 97 F5"_hex;
        QTest::newRow("setSpeed128:extend")
                << dcc::Request::setSpeed126(1930, 42, dcc::Direction::Reverse)
                << "C7 8A 3F 2A 58"_hex;

        QTest::newRow("setFunctions:group1")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group1, 0x10)
                << "C3 3E 90 6D"_hex;
        QTest::newRow("setFunctions:group2")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group2, 0x0f)
                << "C3 3E AF 52"_hex;
        QTest::newRow("setFunctions:group3")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group3, 0x01)
                << "C3 3E B1 4C"_hex;
        QTest::newRow("setFunctions:group4")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group4, 0x02)
                << "C3 3E DE 02 21"_hex;
        QTest::newRow("setFunctions:group5")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group5, 0x04)
                << "C3 3E DF 04 26"_hex;
        QTest::newRow("setFunctions:group6")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group6, 0x08)
                << "C3 3E D8 08 2D"_hex;
        QTest::newRow("setFunctions:group7")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group7, 0x10)
                << "C3 3E D9 10 34"_hex;
        QTest::newRow("setFunctions:group8")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group8, 0x20)
                << "C3 3E DA 20 07"_hex;
        QTest::newRow("setFunctions:group9")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group9, 0x40)
                << "C3 3E DB 40 66"_hex;
        QTest::newRow("setFunctions:group10")
                << dcc::Request::setFunctions(830, dcc::FunctionGroup::Group10, 0x80)
                << "C3 3E DC 80 A1"_hex;

        QTest::newRow("verifyBit:CV29")
                << Request::verifyBit(29, true, 5)
                << "78 1C ED 89"_hex;
        QTest::newRow("verifyBit:CV570")
                << Request::verifyBit(570, true, 7)
                << "7A 39 EF AC"_hex;
        QTest::newRow("verifyByte:CV1")
                << Request::verifyByte(1, 3)
                << "74 00 03 77"_hex;
        QTest::newRow("verifyByte:CV260")
                << Request::verifyByte(260, 82)
                << "75 03 52 24"_hex;
        QTest::newRow("writeByte:CV29")
                << Request::writeByte(29, 48)
                << "7C 1C 30 50"_hex;
        QTest::newRow("writeByte:CV257")
                << Request::writeByte(259, 66)
                << "7D 02 42 3D"_hex;
        QTest::newRow("writeByte:CV1021")
                << Request::writeByte(1021, 3)
                << "7F FC 03 80"_hex;
    }

    void testRequestGenerator()
    {
        const QFETCH(Request, request);
        const QFETCH(QByteArray, expectedBytes);

        QCOMPARE(request.toByteArray().toHex(' '), expectedBytes.toHex(' '));
    }
};

} // namespace lmrs::core::dcc::tests

QTEST_MAIN(lmrs::core::dcc::tests::RequestTest)

#include "tst_dccrequest.moc"
