#include <lmrs/core/continuation.h>

#include <QtTest>

namespace lmrs::core::tests {

class ContinuationTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testContructors_data()
    {
        QTest::addColumn<std::function<Continuation(int)>>("function");
        QTest::addColumn<ContinuationCallback<int>>("callback");
        QTest::addColumn<Continuation>("expectedContinuation");
        QTest::addColumn<bool>("expectCallable");

        const auto retry = [](int) { return Continuation::Retry; };
        const auto procceed = [](int) { return Continuation::Proceed; };

        QTest::newRow("empty")
                << std::function<Continuation(int)>{}
                << ContinuationCallback<int>{}
                << Continuation::Proceed
                << false;
        QTest::newRow("retry")
                << std::function<Continuation(int)>{retry}
                << ContinuationCallback<int>{retry}
                << Continuation::Retry
                << true;
        QTest::newRow("procceed")
                << std::function<Continuation(int)>{procceed}
                << ContinuationCallback<int>{procceed}
                << Continuation::Proceed
                << true;
    }

    void testContructors()
    {
        QFETCH(std::function<Continuation(int)>, function);
        QFETCH(ContinuationCallback<int>, callback);
        QFETCH(Continuation, expectedContinuation);
        QFETCH(bool, expectCallable);

        QCOMPARE(static_cast<bool>(function), expectCallable);
        QCOMPARE(callIfDefined(Continuation::Proceed, function, 0), expectedContinuation);

        QCOMPARE(callback.retryCount(), 0);
        QCOMPARE(callback.retryLimit(), 3);
        QCOMPARE(static_cast<bool>(callback), expectCallable);
        QCOMPARE(callIfDefined(Continuation::Proceed, callback, 0), expectedContinuation);

        const auto firstRetry = callback.retry();
        QCOMPARE(firstRetry.retryCount(), expectCallable ? 1 : 0);
        QCOMPARE(firstRetry.retryLimit(), 3);
        QCOMPARE(static_cast<bool>(firstRetry), expectCallable);
        QCOMPARE(callIfDefined(Continuation::Proceed, firstRetry, 0), expectedContinuation);

        const auto secondRetry = firstRetry.retry();
        QCOMPARE(secondRetry.retryCount(), expectCallable ? 2 : 0);
        QCOMPARE(secondRetry.retryLimit(), 3);
        QCOMPARE(static_cast<bool>(secondRetry), expectCallable);
        QCOMPARE(callIfDefined(Continuation::Proceed, secondRetry, 0), expectedContinuation);

        const auto thirdRetry = secondRetry.retry();
        QCOMPARE(thirdRetry.retryCount(), expectCallable ? 3 : 0);
        QCOMPARE(thirdRetry.retryLimit(), 3);
        QCOMPARE(static_cast<bool>(thirdRetry), expectCallable);
        QCOMPARE(callIfDefined(Continuation::Proceed, thirdRetry, 0), expectedContinuation);

        const auto failingRetry = thirdRetry.retry();
        QCOMPARE(failingRetry.retryCount(), 0);
        QCOMPARE(failingRetry.retryLimit(), 3);
        QCOMPARE(static_cast<bool>(failingRetry), false);
        QCOMPARE(callIfDefined(Continuation::Abort, failingRetry, 0), Continuation::Abort);
    }
};

} // namespace lmrs::core::tests

QTEST_MAIN(lmrs::core::tests::ContinuationTest)

#include "tst_continuation.moc"
