#include <lmrs/core/userliterals.h>
#include <lmrs/esu/lp2stream.h>

#include <QtTest>

namespace lmrs::esu::lp2::tests {

class StreamTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testStreamReader()
    {
        auto reader = StreamReader{{}};

        QVERIFY(reader.isAtEnd());
        QVERIFY(!reader.readNext());
        QVERIFY(reader.isAtEnd());

        QCOMPARE(reader.frame(), QByteArray{});

        reader.addData("ABC"_qba);

        QVERIFY(!reader.isAtEnd());
        QVERIFY(!reader.readNext());
        QVERIFY(reader.isAtEnd());

        QCOMPARE(reader.frame(), QByteArray{});

        reader.addData("7f"_hex);

        QVERIFY(!reader.isAtEnd());
        QVERIFY(!reader.readNext());
        QVERIFY(!reader.isAtEnd());

        QCOMPARE(reader.frame(), QByteArray{});

        reader.addData("7f 01 02 03 80 7f 80 80 80 81 81"_hex);

        QVERIFY(!reader.isAtEnd());
        QVERIFY(reader.readNext());
        QVERIFY(reader.isAtEnd());

        QCOMPARE(reader.frame(), "01 02 03 7f 80 81"_hex);

        reader.addData("7f 7f 01 02 03 81 7f 7f 04 05 06 81 7f 7f 07 08 09 81"_hex);

        QVERIFY(!reader.isAtEnd());
        QVERIFY(reader.readNext());
        QVERIFY(!reader.isAtEnd());

        QCOMPARE(reader.frame(), "01 02 03"_hex);

        QVERIFY(!reader.isAtEnd());
        QVERIFY(reader.readNext());
        QVERIFY(!reader.isAtEnd());

        QCOMPARE(reader.frame(), "04 05 06"_hex);

        QVERIFY(!reader.isAtEnd());
        QVERIFY(reader.readNext());
        QVERIFY(reader.isAtEnd());

        QCOMPARE(reader.frame(), "07 08 09"_hex);

        reader.addData("7f 7f 01 02 03 81 04 05 06 7f 7f 07 08 09 81"_hex);

        QVERIFY(!reader.isAtEnd());
        QVERIFY(reader.readNext());
        QVERIFY(!reader.isAtEnd());

        QCOMPARE(reader.frame(), "01 02 03"_hex);

        QVERIFY(!reader.isAtEnd());
        QVERIFY(reader.readNext());
        QVERIFY(reader.isAtEnd());

        QCOMPARE(reader.frame(), "07 08 09"_hex);
    }

    void testStreamWriter_data()
    {
        QTest::addColumn<QByteArray>("frame");
        QTest::addColumn<QByteArray>("expectedStream");

        QTest::newRow("empty") << ""_hex << "7f 7f 81"_hex;
        QTest::newRow("simple") << "01 02 03"_hex << "7f 7f 01 02 03 81"_hex;
        QTest::newRow("escaping") << "01 02 03 7f 80 81"_hex << "7f 7f 01 02 03 80 7f 80 80 80 81 81"_hex;
    }

    void testStreamWriter()
    {
        QFETCH(QByteArray, frame);
        QFETCH(QByteArray, expectedStream);

        auto buffer = QBuffer{};

        QVERIFY2(buffer.open(QBuffer::WriteOnly), qUtf8Printable(buffer.errorString()));

        StreamWriter{&buffer}.writeFrame(std::move(frame));

        QCOMPARE(buffer.data(), expectedStream);
    }
};

} // namespace lmrs::esu::lp2::tests

QTEST_MAIN(lmrs::esu::lp2::tests::StreamTest)

#include "tst_lp2stream.moc"
