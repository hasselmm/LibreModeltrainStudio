#include <lmrs/widgets/speeddial.h>

#include <QDial>
#include <QtTest>

namespace lmrs::widgets::tests {

namespace {

auto toIntList(QList<QVariantList> input)
{
    auto output = QList<int>{};
    std::transform(input.begin(), input.end(), std::back_inserter(output),
                   [](const QVariantList &list) { return list.first().toInt(); });
    return output;
}

} // namespace

class SpeedDialTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    using Bias = SpeedDial::Bias;

private slots:
    void testInitialValues()
    {
        const auto speedDial = SpeedDial{};
        const auto rawDial = speedDial.findChild<QDial *>();

        QVERIFY(rawDial);

        QCOMPARE(speedDial.centerSteps(), 20);
        QCOMPARE(speedDial.valueSteps(), 100);
        QCOMPARE(speedDial.endSteps(),    10);

        QCOMPARE(speedDial.value(), 0);
        QCOMPARE(speedDial.bias(), Bias::Neutral);

        QCOMPARE(rawDial->value(),      0);
        QCOMPARE(rawDial->minimum(), -130);
        QCOMPARE(rawDial->maximum(),  130);
    }


    void testUserInput_data()
    {
        QTest::addColumn<int>("inputValue");
        QTest::addColumn<int>("expectedRawValue");
        QTest::addColumn<int>("expectedSpeedValue");
        QTest::addColumn<Bias>("expectedBias");
        QTest::addColumn<QList<int>>("expectedRawSignals");
        QTest::addColumn<QList<int>>("expectedSpeedSignals");

        QTest::newRow("zero")            <<    0 <<    0 <<   0 << Bias::Neutral  << QList<int>{    } << QList<int>{   };
        QTest::newRow("center:positive") <<    5 <<    1 <<   0 << Bias::Positive << QList<int>{   5} << QList<int>{   };
        QTest::newRow("center:negative") <<  -15 <<   -1 <<   0 << Bias::Negative << QList<int>{ -15} << QList<int>{   };
        QTest::newRow("value:positive")  <<   25 <<   25 <<   5 << Bias::Positive << QList<int>{  25} << QList<int>{  5};
        QTest::newRow("value:negative")  <<  -35 <<  -35 << -15 << Bias::Negative << QList<int>{ -35} << QList<int>{-15};
        QTest::newRow("end:positive")    <<  125 <<  120 <<  99 << Bias::Positive << QList<int>{ 125} << QList<int>{ 99};
        QTest::newRow("end:negative")    << -135 << -120 << -99 << Bias::Negative << QList<int>{-130} << QList<int>{-99};
    }

    void testUserInput()
    {
        QFETCH(int, inputValue);
        QFETCH(int, expectedRawValue);
        QFETCH(int, expectedSpeedValue);
        QFETCH(Bias, expectedBias);
        QFETCH(QList<int>, expectedRawSignals);
        QFETCH(QList<int>, expectedSpeedSignals);

        const auto speedDial = SpeedDial{};
        const auto rawDial = speedDial.findChild<QDial *>();

        QVERIFY(rawDial);

        auto rawValueChanged = QSignalSpy{rawDial, &QDial::valueChanged};
        auto speedValueChanged = QSignalSpy{&speedDial, &SpeedDial::valueChanged};

        QCOMPARE(rawDial->value(), 0);
        QCOMPARE(speedDial.value(), 0);
        QCOMPARE(speedDial.bias(), Bias::Neutral);

        rawDial->setValue(inputValue); // <- the actual test <- <<------------------------------------------------------

        QCOMPARE(rawDial->value(), expectedRawValue);
        QCOMPARE(speedDial.value(), expectedSpeedValue);
        QCOMPARE(speedDial.bias(), expectedBias);

        QCOMPARE(toIntList(rawValueChanged), expectedRawSignals);
        QCOMPARE(toIntList(speedValueChanged), expectedSpeedSignals);
    }

    void testSetValue_data()
    {
        QTest::addColumn<int>("inputValue");
        QTest::addColumn<Bias>("inputBias");
        QTest::addColumn<int>("expectedRawValue");
        QTest::addColumn<int>("expectedSpeedValue");
        QTest::addColumn<Bias>("expectedBias");
        QTest::addColumn<QList<int>>("expectedRawSignals");
        QTest::addColumn<QList<int>>("expectedSpeedSignals");

        QTest::newRow("zero-value:neutral")    <<    0 << Bias::Neutral  <<    0 <<   0 << Bias::Neutral  << QList<int>{         } << QList<int>{   };
        QTest::newRow("zero-value:positive")   <<    0 << Bias::Positive <<    1 <<   0 << Bias::Positive << QList<int>{        1} << QList<int>{   };
        QTest::newRow("zero-value:negative")   <<    0 << Bias::Negative <<   -1 <<   0 << Bias::Negative << QList<int>{       -1} << QList<int>{   };
        QTest::newRow("valid-value:positive")  <<   25 << Bias::Positive <<   45 <<  25 << Bias::Positive << QList<int>{       45} << QList<int>{ 25};
        QTest::newRow("valid-value:negative")  <<  -50 << Bias::Negative <<  -70 << -50 << Bias::Negative << QList<int>{ 70,  -70} << QList<int>{-50};
        QTest::newRow("out-of-range:positive") <<  100 << Bias::Positive <<  120 <<  99 << Bias::Positive << QList<int>{      120} << QList<int>{ 99};
        QTest::newRow("out-of-range:negative") << -120 << Bias::Negative << -120 << -99 << Bias::Negative << QList<int>{120, -120} << QList<int>{-99};
    }

    void testSetValue()
    {
        QFETCH(int, inputValue);
        QFETCH(Bias, inputBias);
        QFETCH(int, expectedRawValue);
        QFETCH(int, expectedSpeedValue);
        QFETCH(Bias, expectedBias);
        QFETCH(QList<int>, expectedRawSignals);
        QFETCH(QList<int>, expectedSpeedSignals);

        auto speedDial = SpeedDial{};
        const auto rawDial = speedDial.findChild<QDial *>();

        QVERIFY(rawDial);

        auto rawValueChanged = QSignalSpy{rawDial, &QDial::valueChanged};
        auto speedValueChanged = QSignalSpy{&speedDial, &SpeedDial::valueChanged};

        QCOMPARE(rawDial->value(), 0);
        QCOMPARE(speedDial.value(), 0);
        QCOMPARE(speedDial.bias(), Bias::Neutral);

        speedDial.setValue(inputValue, inputBias); // <- the actual test <- <<------------------------------------------

        QCOMPARE(rawDial->value(), expectedRawValue);
        QCOMPARE(speedDial.value(), expectedSpeedValue);
        QCOMPARE(speedDial.bias(), expectedBias);

        QCOMPARE(toIntList(rawValueChanged), expectedRawSignals);
        QCOMPARE(toIntList(speedValueChanged), expectedSpeedSignals);
    }

    void testSetBias_data()
    {
        QTest::addColumn<int>("value");
        QTest::addColumn<Bias>("bias1");
        QTest::addColumn<Bias>("bias2");
        QTest::addColumn<int>("expectedValue1");
        QTest::addColumn<Bias>("expectedBias1");
        QTest::addColumn<int>("expectedValue2");
        QTest::addColumn<Bias>("expectedBias2");

        QTest::newRow("zero:neutral:neutral")            <<  0 << Bias::Neutral << Bias::Neutral  <<  0 << Bias::Neutral  <<  0 << Bias::Neutral;
        QTest::newRow("zero:neutral:positive")           <<  0 << Bias::Neutral << Bias::Positive <<  0 << Bias::Neutral  <<  0 << Bias::Positive;
        QTest::newRow("zero:neutral:negative")           <<  0 << Bias::Neutral << Bias::Negative <<  0 << Bias::Neutral  <<  0 << Bias::Negative;

        QTest::newRow("positive-value:neutral:neutral")  <<  1 << Bias::Neutral << Bias::Neutral  <<  1 << Bias::Positive <<  1 << Bias::Positive;
        QTest::newRow("positive-value:neutral:positive") <<  2 << Bias::Neutral << Bias::Positive <<  2 << Bias::Positive <<  2 << Bias::Positive;
        QTest::newRow("positive-value:neutral:negative") <<  3 << Bias::Neutral << Bias::Negative <<  3 << Bias::Positive << -3 << Bias::Negative;

        QTest::newRow("negative-value:neutral:neutral")  << -4 << Bias::Neutral << Bias::Neutral  << -4 << Bias::Negative << -4 << Bias::Negative;
        QTest::newRow("negative-value:neutral:positive") << -5 << Bias::Neutral << Bias::Positive << -5 << Bias::Negative <<  5 << Bias::Positive;
        QTest::newRow("negative-value:neutral:negative") << -6 << Bias::Neutral << Bias::Negative << -6 << Bias::Negative << -6 << Bias::Negative;
    }

    void testSetBias()
    {
        QFETCH(int,  value);
        QFETCH(Bias, bias1);
        QFETCH(Bias, bias2);
        QFETCH(int,  expectedValue1);
        QFETCH(int,  expectedValue2);
        QFETCH(Bias, expectedBias1);
        QFETCH(Bias, expectedBias2);

        auto speedDial = SpeedDial{};

        speedDial.setValue(value, bias1);

        QCOMPARE(speedDial.value(), expectedValue1);
        QCOMPARE(speedDial.bias(), expectedBias1);

        speedDial.setBias(bias2);

        QCOMPARE(speedDial.value(), expectedValue2);
        QCOMPARE(speedDial.bias(), expectedBias2);
    }
};

} // namespace lmrs::core::tests

QTEST_MAIN(lmrs::widgets::tests::SpeedDialTest)

#include "tst_speeddial.moc"
