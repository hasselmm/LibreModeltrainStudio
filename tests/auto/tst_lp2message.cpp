#include <lmrs/core/userliterals.h>
#include <lmrs/esu/lp2constants.h>
#include <lmrs/esu/lp2message.h>

#include <QtTest>

namespace lmrs::esu::lp2::tests {

class MessageTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testMetaTypesDefined()
    {
        QVERIFY(QMetaType::fromType<InterfaceApplicationType>().name());
        QVERIFY(QMetaType::fromType<InterfaceApplicationTypes>().name());
        QVERIFY(QMetaType::fromType<InterfaceFlag>().name());
        QVERIFY(QMetaType::fromType<InterfaceFlags>().name());
        QVERIFY(QMetaType::fromType<InterfaceInfo>().name());
    }

    void notImplemented()
    {
        QSKIP("This test is not implemented yet"); // FIXME: implement this test
    }
};

} // namespace lmrs::esu::lp2::tests

QTEST_MAIN(lmrs::esu::lp2::tests::MessageTest)

#include "tst_lp2message.moc"
