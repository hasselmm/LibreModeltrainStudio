#include <lmrs/core/algorithms.h>

#include <QtTest>

namespace lmrs::core::tests {

class AlgorithmsTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testQString()
    {
        QCOMPARE(coalesce(u"A"_qs), u"A"_qs);
        QCOMPARE(coalesce(QString{}, u"B"_qs), u"B"_qs);
    }
};

} // namespace lmrs::core::tests

QTEST_MAIN(lmrs::core::tests::AlgorithmsTest)

#include "tst_algorithms.moc"
