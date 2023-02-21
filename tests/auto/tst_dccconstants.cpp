#include <lmrs/core/algorithms.h>
#include <lmrs/core/dccconstants.h>

#include <QtTest>

namespace lmrs::core::dcc::tests {

constexpr auto operator+(VehicleVariable lhs, ExtendedVariableIndex rhs)
{
    return value(lhs) + rhs;
}

class ConstantsTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testFullVariableName_data()
    {
        QTest::addColumn<ExtendedVariableIndex>("variable");
        QTest::addColumn<QString>("expectedName");

        static const auto newRow = [](auto variable, auto expectedName) {
            if (hasExtendedPage(variable)) {
                QTest::addRow("CV%d.%d", value(variableIndex(variable)), value(extendedPage(variable)))
                        << static_cast<ExtendedVariableIndex>(value(variable))
                        << expectedName;
            } else if (hasSusiPage(variable)) {
                QTest::addRow("CV%d.%d", value(variableIndex(variable)), value(susiPage(variable)))
                        << static_cast<ExtendedVariableIndex>(value(variable))
                        << expectedName;
            } else {
                QTest::addRow("CV%d", value(variable))
                        << static_cast<ExtendedVariableIndex>(value(variable))
                        << expectedName;
            }
        };

        newRow(VehicleVariable::Manufacturer, "Manufacturer");
        newRow(VehicleVariable::VendorUnique1Begin, "VendorUnique1.0");
        newRow(VehicleVariable::VendorUnique1Begin + 2, "VendorUnique1.2");
        newRow(VehicleVariable::SpeedTableBegin, "SpeedTable0");
        newRow(VehicleVariable::SpeedTableBegin + 2, "SpeedTable2");
        newRow(VehicleVariable::ExtendedBegin, "Extended0");
        newRow(VehicleVariable::ExtendedBegin + 2, "Extended2");
        newRow(VehicleVariable::Susi1Begin, "Susi1.0");
        newRow(VehicleVariable::Susi1Begin + 2, "Susi1.2");

        newRow(extendedVariable(13, 0), "Extended13\u2080");
        newRow(extendedVariable(23, 42), "Extended23\u2084\u2082");

        newRow(VehicleVariable::RailComManufacturer, "RailComManufacturer");
        newRow(VehicleVariable::RailComPlusIcon, "RailComPlusIcon");
        newRow(VehicleVariable::RailComPlusName, "RailComPlusName0");
        newRow(VehicleVariable::RailComPlusName + 2, "RailComPlusName2");

        newRow(VehicleVariable::Susi1Manufacturer, "Susi1Manufacturer");
        newRow(VehicleVariable::Susi1MajorVersion, "Susi1MajorVersion");
        newRow(VehicleVariable::Susi2MinorVersion, "Susi2MinorVersion");
        newRow(VehicleVariable::Susi3SusiVersion, "Susi3SusiVersion");

        newRow(susiVariable(910, 0), "Susi1.10\u2080");
        newRow(susiVariable(911, 1), "Susi1.11\u2081");
    }

    void testFullVariableName()
    {
        const QFETCH(ExtendedVariableIndex, variable);
        const QFETCH(QString, expectedName);

        QCOMPARE(fullVariableName(variable), expectedName);
    }

    void testDetectorAddress_data()
    {
        QTest::addColumn<bool>("expectUniqueAddresses");
        QTest::addColumn<rm::DetectorAddress::Type>("expectedType");
        QTest::addColumn<rm::DetectorAddress>("firstAddress");
        QTest::addColumn<rm::DetectorAddress>("secondAddress");

        for (const auto &entry: QMetaTypeId<rm::DetectorAddress::Type>{}) {
            switch (entry.value()) {
            case rm::DetectorAddress::Type::Invalid:
                QTest::newRow(entry.key()) << false << entry.value()
                                           << rm::DetectorAddress{}
                                           << rm::DetectorAddress{};
                break;

            case rm::DetectorAddress::Type::CanNetwork:
                QTest::newRow(entry.key()) << true << entry.value()
                                           << rm::DetectorAddress::forCanNetwork(0xdc3f)
                                           << rm::DetectorAddress::forCanNetwork(0xdc40);
                break;

            case rm::DetectorAddress::Type::CanModule:
                QTest::newRow(entry.key()) << true << entry.value()
                                           << rm::DetectorAddress::forCanModule(0xdc3f, 1)
                                           << rm::DetectorAddress::forCanModule(0xdc3f, 2);
                break;

            case rm::DetectorAddress::Type::CanPort:
                QTest::newRow(entry.key()) << true << entry.value()
                                           << rm::DetectorAddress::forCanPort(1, 1, 1)
                                           << rm::DetectorAddress::forCanPort(1, 1, 2);
                break;

            case rm::DetectorAddress::Type::LissyModule:
                QTest::newRow(entry.key()) << true << entry.value()
                                           << rm::DetectorAddress::forLissyModule(0)
                                           << rm::DetectorAddress::forLissyModule(1);
                break;

            case rm::DetectorAddress::Type::LoconetSIC:
                QTest::newRow(entry.key()) << false << entry.value()
                                           << rm::DetectorAddress::forLoconetSIC()
                                           << rm::DetectorAddress::forLoconetSIC();
                break;

            case rm::DetectorAddress::Type::LoconetModule:
                QTest::newRow(entry.key()) << true << entry.value()
                                           << rm::DetectorAddress::forLoconetModule(0)
                                           << rm::DetectorAddress::forLoconetModule(1);
                break;

            case rm::DetectorAddress::Type::RBusGroup:
                QTest::newRow(entry.key()) << true << entry.value()
                                           << rm::DetectorAddress::forRBusGroup(0)
                                           << rm::DetectorAddress::forRBusGroup(1);
                break;

            case rm::DetectorAddress::Type::RBusModule:
                QTest::newRow(entry.key()) << true << entry.value()
                                           << rm::DetectorAddress::forRBusModule(1)
                                           << rm::DetectorAddress::forRBusModule(2);
                break;

            case rm::DetectorAddress::Type::RBusPort:
                QTest::newRow(entry.key()) << true << entry.value()
                                           << rm::DetectorAddress::forRBusPort(1, 1)
                                           << rm::DetectorAddress::forRBusPort(1, 2);
                break;
            }
        }
    }

    void testDetectorAddress()
    {
        const QFETCH(bool, expectUniqueAddresses);
        const QFETCH(rm::DetectorAddress::Type, expectedType);
        const QFETCH(rm::DetectorAddress, firstAddress);
        const QFETCH(rm::DetectorAddress, secondAddress);

        qInfo() << firstAddress;

        QCOMPARE(firstAddress.type(), expectedType);
        QCOMPARE(secondAddress.type(), expectedType);

        QVERIFY(firstAddress == firstAddress);
        QVERIFY(qHash(firstAddress) == qHash(firstAddress));

        if (expectedType != rm::DetectorAddress::Type::Invalid)
            QVERIFY(firstAddress != rm::DetectorAddress{});
        else
            QVERIFY(firstAddress == rm::DetectorAddress{});

        if (expectUniqueAddresses) {
            QVERIFY(firstAddress != secondAddress);
            QVERIFY(secondAddress != firstAddress);
            QVERIFY(secondAddress != rm::DetectorAddress{});

            QVERIFY(qHash(firstAddress) != qHash(secondAddress));
            QVERIFY(qHash(secondAddress) != qHash(firstAddress));
        } else {
            QVERIFY(firstAddress == secondAddress);
            QVERIFY(secondAddress == firstAddress);

            QVERIFY(qHash(firstAddress) == qHash(secondAddress));
            QVERIFY(qHash(secondAddress) == qHash(firstAddress));
        }
    }
};

} // namespace lmrs::core::dcc::tests

QTEST_MAIN(lmrs::core::dcc::tests::ConstantsTest)

#include "tst_dccconstants.moc"
