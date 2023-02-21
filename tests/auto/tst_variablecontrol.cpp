#include <lmrs/core/algorithms.h>
#include <lmrs/core/device.h>
#include <lmrs/core/typetraits.h>

#include <QtTest>

// FIXME: move this debug output functions to a proper place
namespace QTest {
template<> char *toString(const lmrs::core::dcc::ExtendedVariableIndex &x) { return toString(x.value); }
template<> char *toString(const lmrs::core::dcc::VariableValue &x) { return toString(x.value); }
}

//QDebug operator<<(QDebug debug, lmrs::core::dcc::VehicleAddress i) { return debug << "Vehicle/Address(" << i.value << ')'; }
//QDebug operator<<(QDebug debug, lmrs::core::dcc::ExtendedVariableIndex i) { return debug << i.value; }
//QDebug operator<<(QDebug debug, lmrs::core::dcc::VariableValue v) { return debug << v.value; }

QDebug operator<<(QDebug debug, lmrs::core::Result<lmrs::core::dcc::VariableValue> r) { return debug << r.error << "/" << r.value; }

namespace lmrs::core::tests {

class MockVariableControl : public VariableControl
{
    Q_OBJECT

public:
    explicit MockVariableControl()
        : VariableControl{nullptr}
    {
        m_variables.insert({0, value(dcc::VehicleVariable::BasicAddress)},         {Error::NoError,   3});
        m_variables.insert({0, value(dcc::VehicleVariable::ExtendedAddressHigh)},  {Error::NoError,  11});
        m_variables.insert({0, value(dcc::VehicleVariable::ExtendedAddressLow)},   {Error::NoError, 184});
        m_variables.insert({0, value(dcc::VehicleVariable::Configuration)},        {Error::NoError,  42});
    }

    Device *device() const override { return nullptr; }
    Features features() const override { return {}; }

    void readVariable(dcc::VehicleAddress address, dcc::VariableIndex variable,
                      ContinuationCallback<VariableValueResult> callback) override
    {
        QTimer::singleShot(0, this, [this, address, variable, callback] {
            switch (callIfDefined(Continuation::Proceed, callback, m_variables[{address, variable}])) {
            case core::Continuation::Retry:
                if (const auto next = callback.retry())
                    readVariable(address, variable, next);

                break;

            case core::Continuation::Proceed:
            case core::Continuation::Abort:
                break;
            }

            emit readVariableCalled(address, variable);
        });
    }

    void writeVariable(dcc::VehicleAddress address, dcc::VariableIndex variable, dcc::VariableValue value,
                       core::ContinuationCallback<VariableValueResult> callback) override
    {
        QTimer::singleShot(0, this, [this, address, variable, value, callback] {
            m_variables[{address, variable}] = {Error::NoError, value};

            switch (callIfDefined(Continuation::Proceed, callback, m_variables[{address, variable}])) {
            case core::Continuation::Retry:
                if (const auto next = callback.retry())
                    writeVariable(address, variable, value, next);

                break;

            case core::Continuation::Proceed:
            case core::Continuation::Abort:
                break;
            }

            emit writeVariableCalled(address, variable, value);
        });
    }

signals:
    void readVariableCalled(dcc::VehicleAddress address, dcc::VariableIndex variable);
    void writeVariableCalled(dcc::VehicleAddress address, dcc::VariableIndex variable, dcc::VariableValue value);

private:
    QHash<QPair<dcc::VehicleAddress, dcc::VariableIndex>, VariableValueResult> m_variables;
};

class VariableControlTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testReadVariable()
    {
        auto control = MockVariableControl{};
        auto spy = QSignalSpy{&control, &MockVariableControl::readVariableCalled};
        auto results = QVariantList{};

        const auto makeQuery = [](dcc::VehicleAddress address, dcc::VariableIndex variable) {
            return QList{QVariant::fromValue(address), QVariant::fromValue(variable)};
        };

        const auto collectResult = [&results](auto r) {
            if (r.error == Error::NoError)
                results.append(static_cast<int>(r.value));
            else
                results.append(QVariant::fromValue(r.error));

            return Continuation::Proceed;
        };

        QCOMPARE(spy, QList<QVariantList>{});

        control.readVariable(0, 1, collectResult);
        QVERIFY(spy.wait());

        QCOMPARE(spy, QList<QVariantList>{makeQuery(0, 1)});
        QCOMPARE(results, QVariantList{QVariant::fromValue(3)});
    }

    void testReadExtendedVariables()
    {
        const QList<QVariantList> expectedVariableReads = {
            {QVariant::fromValue(dcc::VehicleAddress{0}), QVariant::fromValue(dcc::VariableIndex{1})},
            {QVariant::fromValue(dcc::VehicleAddress{0}), QVariant::fromValue(dcc::VariableIndex{17})},
            {QVariant::fromValue(dcc::VehicleAddress{0}), QVariant::fromValue(dcc::VariableIndex{18})},
            {QVariant::fromValue(dcc::VehicleAddress{0}), QVariant::fromValue(dcc::VariableIndex{29})},
        };

        const QList<QPair<dcc::ExtendedVariableIndex, MockVariableControl::VariableValueResult>> expectedMultiCallResults = {
            {1,  {Error::NoError,   3}},
            {17, {Error::NoError,  11}},
            {18, {Error::NoError, 184}},
            {29, {Error::NoError,  42}},
        };

        const MockVariableControl::ExtendedVariableResults expectedSingleCallResults = {
            expectedMultiCallResults.begin(), expectedMultiCallResults.end()
        };

        auto control = MockVariableControl{};
        auto actualQueries = QSignalSpy{&control, &MockVariableControl::readVariableCalled};
        auto actualMultiCallResults = decltype(expectedMultiCallResults){};

        control.readExtendedVariables(0, {1, 17, 18, 29}, [&](auto variable, auto result) {
            actualMultiCallResults.append({variable, std::move(result)});
            return Continuation::Proceed;
        });

        while (actualQueries.size() != expectedVariableReads.size())
            QVERIFY(actualQueries.wait());

        QCOMPARE(QList{actualQueries}, expectedVariableReads);
        QCOMPARE(actualMultiCallResults, expectedMultiCallResults);

        actualQueries.clear();
        auto actualSingleCallResults = decltype(expectedSingleCallResults){};

        control.readExtendedVariables(0, {1, 17, 18, 29}, [&](auto results) {
            actualSingleCallResults = std::move(results);
        });

        while (actualQueries.size() != expectedVariableReads.size())
            QVERIFY(actualQueries.wait());

        QCOMPARE(actualQueries, expectedVariableReads);
        QCOMPARE(actualSingleCallResults, expectedSingleCallResults);
    }

    void testContinuationHandling()
    {
        QFAIL("This test is not implemented yet"); // FIXME: implement this test
    }
};

} // namespace lmrs::core::tests

QTEST_MAIN(lmrs::core::tests::VariableControlTest)

#include "tst_variablecontrol.moc"
