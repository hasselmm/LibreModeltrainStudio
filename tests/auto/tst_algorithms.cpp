#include <lmrs/core/algorithms.h>

#include <QtTest>

namespace lmrs::core::tests {

namespace {

template<typename T>
void testMetaEnumIterator()
{
    auto actualKeys = QByteArrayList{};
    auto actualValues = QList<T>{};

    for (const auto &entry: QMetaTypeId<T>{}) {
        QVERIFY(entry.key());
        QCOMPARE(entry.index(), actualKeys.count());
        actualKeys.append(entry.key());

        QCOMPARE(entry.index(), actualValues.count());
        actualValues.append(entry.value());
    }

    const auto expectedKeys = QByteArrayList{"A", "B", "C"};
    QCOMPARE(actualKeys, expectedKeys);

    const auto expectedValues = QList{T::A, T::B, T::C};
    QCOMPARE(actualValues, expectedValues);
}

using FunctionPointer = void (*)();

} // namespace

class AlgorithmsTest : public QObject
{
    Q_OBJECT

public:
    enum class TestEnum { A, B, C };

    Q_ENUM(TestEnum)

    enum class TestFlag { A = 1, B = 2, C = 4 };

    Q_FLAG(TestFlag)
    Q_DECLARE_FLAGS(TestFlags, TestFlag)

    using QObject::QObject;

private slots:
    void testMetaEnumUtils();

    void testMetaEnumIterator_data();
    void testMetaEnumIterator();

    void testFlagsIterator_data();
    void testFlagsIterator();

    void testCoalesce();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AlgorithmsTest::TestFlags)

void AlgorithmsTest::testMetaEnumUtils()
{
    QCOMPARE(keyCount<TestEnum>(), 3);
    QCOMPARE(key(TestEnum::A), "A");
}

void AlgorithmsTest::testMetaEnumIterator_data()
{
    QTest::addColumn<FunctionPointer>("routine");

    QTest::newRow("enum") << &tests::testMetaEnumIterator<TestEnum>;
    QTest::newRow("flag") << &tests::testMetaEnumIterator<TestFlag>;
}

void AlgorithmsTest::testMetaEnumIterator()
{
    QFETCH(FunctionPointer, routine);
    routine();
}

void AlgorithmsTest::testFlagsIterator_data()
{
    QTest::addColumn<TestFlags>("actualFlags");
    QTest::addColumn<QList<TestFlag>>("expectedValues");

    QTest::newRow("null") << TestFlags{}
                          << QList<TestFlag>{};
    QTest::newRow("one") << TestFlags{TestFlag::A}
                         << QList<TestFlag>{TestFlag::A};
    QTest::newRow("two") << TestFlags{TestFlag::A | TestFlag::B}
                         << QList<TestFlag>{TestFlag::A, TestFlag::B};
    QTest::newRow("three") << TestFlags{TestFlag::A | TestFlag::B | TestFlag::C}
                           << QList<TestFlag>{TestFlag::A, TestFlag::B, TestFlag::C};
}

void AlgorithmsTest::testFlagsIterator()
{
    QFETCH(TestFlags, actualFlags);
    QFETCH(QList<TestFlag>, expectedValues);

    auto actualValues = QList<TestFlag>{};

    for (const auto value: actualFlags)
        actualValues.append(value);

    QCOMPARE(actualValues, expectedValues);
}

void AlgorithmsTest::testCoalesce()
{
    QCOMPARE(coalesce(u"A"_qs), u"A"_qs);
    QCOMPARE(coalesce(QString{}, u"B"_qs), u"B"_qs);
}

} // namespace lmrs::core::tests

QTEST_MAIN(lmrs::core::tests::AlgorithmsTest)

#include "tst_algorithms.moc"
