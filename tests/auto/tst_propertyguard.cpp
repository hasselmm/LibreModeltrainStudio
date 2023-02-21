#include <lmrs/core/propertyguard.h>

#include <QtTest>

namespace lmrs::core::tests {

class PropertyGuardTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    int testProperty() const { return m_testProperty; }

signals:
    void signalWithValue(int);
    void signalWithoutValue();

private slots:
    void testNotify_data()
    {
        QTest::addColumn<SignalSpyFactory>("createSignalSpy");
        QTest::addColumn<PropertyGuardFactory>("createPropertyGuard");
        QTest::addColumn<QList<QVariantList>>("expectedNotifications");
        QTest::addColumn<bool>("revertChange");

        QTest::newRow("signal-with-value:preserve-change")
                << SignalSpyFactory{[this] { return QSignalSpy{this, &PropertyGuardTest::signalWithValue}; }}
                << PropertyGuardFactory{[this] { return propertyGuard(this, &PropertyGuardTest::testProperty,
                                                                      &PropertyGuardTest::signalWithValue); }}
                << QList<QVariantList>{{1}}
                << false;

        QTest::newRow("signal-with-value:revert-change")
                << SignalSpyFactory{[this] { return QSignalSpy{this, &PropertyGuardTest::signalWithValue}; }}
                << PropertyGuardFactory{[this] { return propertyGuard(this, &PropertyGuardTest::testProperty,
                                                                      &PropertyGuardTest::signalWithValue); }}
                << QList<QVariantList>{}
                << true;

        QTest::newRow("signal-without-value:preserve-change")
                << SignalSpyFactory{[this] { return QSignalSpy{this, &PropertyGuardTest::signalWithoutValue}; }}
                << PropertyGuardFactory{[this] { return propertyGuard(this, &PropertyGuardTest::testProperty,
                                                                      &PropertyGuardTest::signalWithoutValue); }}
                << QList<QVariantList>{{}}
                << false;

        QTest::newRow("signal-without-value:revert-change")
                << SignalSpyFactory{[this] { return QSignalSpy{this, &PropertyGuardTest::signalWithoutValue}; }}
                << PropertyGuardFactory{[this] { return propertyGuard(this, &PropertyGuardTest::testProperty,
                                                                      &PropertyGuardTest::signalWithoutValue); }}
                << QList<QVariantList>{}
                << true;
    }

    void testNotify()
    {
        QFETCH(SignalSpyFactory, createSignalSpy);
        QFETCH(PropertyGuardFactory, createPropertyGuard);
        QFETCH(QList<QVariantList>, expectedNotifications);
        QFETCH(bool, revertChange);

        auto signalSpy = createSignalSpy();

        // create and verify initial state
        m_testProperty = 0;
        QCOMPARE(testProperty(), 0);
        QCOMPARE(signalSpy.count(), 0);

        if (const auto propertyGuard = createPropertyGuard()) {
            QCOMPARE(testProperty(), 0);
            QCOMPARE(signalSpy.count(), 0);

            // change the property's value, the change shall remain unseen
            m_testProperty = 1;
            QCOMPARE(testProperty(), 1);
            QCOMPARE(signalSpy.count(), 0);

            if (revertChange) {
                // revert the property change if requested
                m_testProperty = 0;
                QCOMPARE(testProperty(), 0);
                QCOMPARE(signalSpy.count(), 0);
            }
        }

        // verify the property change and if it was seen
        if (revertChange) {
            QCOMPARE(testProperty(), 0);
            QCOMPARE(signalSpy.count(), 0);
        } else {
            QCOMPARE(testProperty(), 1);
            QCOMPARE(signalSpy.count(), 1);
        }

        QCOMPARE(signalSpy, expectedNotifications);
    }

private:
    using SignalSpyFactory = std::function<QSignalSpy()>;
    using PropertyGuardFactory = std::function<PropertyGuard<int>()>;

    int m_testProperty = 0;
};

} // namespace lmrs::core::tests

QTEST_MAIN(lmrs::core::tests::PropertyGuardTest)

#include "tst_propertyguard.moc"
