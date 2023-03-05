#include <lmrs/core/staticinit.h>

#include <QtTest>

namespace lmrs::core::tests {

namespace {

static int counter(bool reset = false)
{
    static auto value = 0;

    if (reset) {
        value = 0;
        return 0;
    }

    return ++value;
}

struct BasicInit
{
    BasicInit() { baseInit = counter(); }
    int baseInit = 0;
};

template<class BaseType = BasicInit>
struct StaticInitTester : public StaticInit<StaticInitTester<BaseType>, BaseType>
{
    static void staticConstructor() { staticInit = counter(); }
    StaticInitTester() { finalInit = counter(); }

    static int staticInit;
    int finalInit = 0;
};

template<> int StaticInitTester<BasicInit>::staticInit = 0;

struct OuterStaticInitTester : public StaticInit<OuterStaticInitTester, StaticInitTester<BasicInit>>
{
    static void staticConstructor() { staticInit = counter(); }
    OuterStaticInitTester() { finalInit = counter(); }

    static int staticInit;
    int finalInit = 0;
};

int OuterStaticInitTester::staticInit = 0;

struct InnerStaticInitTester : public StaticInitTester<StaticInit<InnerStaticInitTester, BasicInit>>
{
    using Outer = StaticInitTester<StaticInit<InnerStaticInitTester, BasicInit>>;

    static void staticConstructor() { staticInit = counter(); }
    InnerStaticInitTester() { finalInit = counter(); }

    static int staticInit;
    int finalInit = 0;
};

template<> int InnerStaticInitTester::Outer::staticInit = 0;
int InnerStaticInitTester::staticInit = 0;

} // namespace

class StaticInitTest : public QObject
{
    Q_OBJECT

private slots:
    void testBaseInit()
    {
        StaticInitTester<BasicInit>::staticInit = 0;
        QCOMPARE(counter(true), 0);

        auto staticInitTester = StaticInitTester{};
        QCOMPARE(StaticInitTester<BasicInit>::staticInit, 1);
        QCOMPARE(staticInitTester.baseInit, 2);
        QCOMPARE(staticInitTester.finalInit, 3);
    }

    void testOuterInit()
    {
        InnerStaticInitTester::Outer::staticInit = 0;
        StaticInitTester<BasicInit>::staticInit = 0;
        OuterStaticInitTester::staticInit = 0;
        QCOMPARE(counter(true), 0);

        auto staticInitTester = OuterStaticInitTester{};
        QCOMPARE(InnerStaticInitTester::Outer::staticInit, 0);
        QCOMPARE(OuterStaticInitTester::staticInit, 1);
        QCOMPARE(StaticInitTester<BasicInit>::staticInit, 2);
        QCOMPARE(staticInitTester.baseInit, 3);
        QCOMPARE(staticInitTester.StaticInitTester::finalInit, 4);
        QCOMPARE(staticInitTester.OuterStaticInitTester::finalInit, 5);
    }

    void testInnerInit()
    {
        InnerStaticInitTester::Outer::staticInit = 0;
        StaticInitTester<BasicInit>::staticInit = 0;
        InnerStaticInitTester::staticInit = 0;
        QCOMPARE(counter(true), 0);

        auto staticInitTester = InnerStaticInitTester{};
        QCOMPARE(StaticInitTester<BasicInit>::staticInit, 0);
        QCOMPARE(InnerStaticInitTester::Outer::staticInit, 1);
        QCOMPARE(InnerStaticInitTester::staticInit, 2);
        QCOMPARE(staticInitTester.baseInit, 3);
        QCOMPARE(staticInitTester.StaticInitTester::finalInit, 4);
        QCOMPARE(staticInitTester.InnerStaticInitTester::finalInit, 5);
    }
};

} // namespace lmrs::core::tests

QTEST_MAIN(lmrs::core::tests::StaticInitTest)

#include "tst_staticinit.moc"
