#include <lmrs/core/userliterals.h>
#include <lmrs/esu/lp2message.h>

#include <QtTest>

namespace lmrs::esu::lp2::tests {

class MessageTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void notImplemented()
    {
        QFAIL("This test is not implemented yet"); // FIXME: implement this test
    }
};

} // namespace lmrs::esu::lp2::tests

QTEST_MAIN(lmrs::esu::lp2::tests::MessageTest)

#include "tst_lp2message.moc"
