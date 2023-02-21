#include "mainwindow.h"

#include <lmrs/core/staticinit.h>
#include <lmrs/core/userliterals.h>
#include <lmrs/esu/lp2request.h>

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>
#include <QVersionNumber>

#include <QtEndian>

using namespace esu::programmer;    // FIXME
using namespace lmrs::esu::lp2;  // FIXME

namespace lmrs::programmer {
namespace {

Q_LOGGING_CATEGORY(lcProgrammer, "programmer")

struct KnownSequence
{
    QByteArray request;
    QByteArray response;
};

const QList<KnownSequence> s_ledPlayground = {
    {
        "7f 7f  01  00  00                  81"_hex,
        "7f 7f  02  00  01 00               81"_hex, // cmd 0x00 -> 0x0001
    }, {
        "7f 7f  01  01  01                  81"_hex,
        "7f 7f  02  01  05 00               81"_hex, // cmd 0x01 -> 0x0500
    }, {
        "7f 7f  01  02  4c 00 00 00         81"_hex,
        "7f 7f  02  02  05 00               81"_hex,
    }, {
        "7f 7f  01  03  4c 01 00 00         81"_hex,
        "7f 7f  02  03  05 00               81"_hex,
    }, {
        "7f 7f  01  04  18 05               81"_hex,
        "7f 7f  02  04  05 00               81"_hex,
    }, {
        "7f 7f  01  05  4c 00 10 10         81"_hex, // yellow led
        "7f 7f  02  05  05 00               81"_hex,
    }, {
        "7f 7f  01  06  18 10               81"_hex,
        "7f 7f  02  06  05 00               81"_hex,
    }, {
        "7f 7f  01  07  4c 01 10 10         81"_hex, // green led, on, off
        "7f 7f  02  07  05 00               81"_hex,
    }
};

const QList<KnownSequence> s_readCV = {
    {
        Request::reset().toByteArray(),
        "7f 7f  02  00  01 00               81"_hex, // cmd 0x00 -> 0x0001
    }, {
        Request::interfaceFlags().toByteArray(),
        "7f 7f  02  01  05 00               81"_hex, // cmd 0x01 -> 0x0500
    }, {
        Request::powerOn(PowerSettings{PowerSettings::Service, 40}).toByteArray(),
        "7f 7f  02  02  05 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(0)).toByteArray(),
        "7f 7f  02  03  05 00               81"_hex,
    }, {
        "7f 7f 01 2d 16 02 81"_hex,
        "7f 7f 02 2d 07 00 81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(20)).toByteArray(),
        "7f 7f  02  04  07 00               81"_hex,
    }, {
        "7f 7f 01 2f 12 81"_hex,
        "7f 7f 02 2f 07 00 01 81"_hex,
    }, { // ESSENTIAL!
        Request::setAcknowledgeMode({}).toByteArray(),
        "7f 7f  02  05  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(10)).toByteArray(),
        "7f 7f  02  06  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  07  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::verifyBit(1, 0, 0)).toByteArray(),
        "7f 7f  02  08  07 00 00            81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  09  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::verifyBit(1, 0, 1)).toByteArray(),
        "7f 7f  02  0a  07 00 00            81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  0b  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::verifyBit(1, 0, 2)).toByteArray(),
        "7f 7f  02  0c  07 00 07            81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  0d  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::verifyBit(1, 0, 3)).toByteArray(),
        "7f 7f  02  0e  07 00 07            81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  0f  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::verifyBit(1, 0, 4)).toByteArray(),
        "7f 7f  02  10  07 00 07            81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  11  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::verifyBit(1, 0, 5)).toByteArray(),
        "7f 7f  02  12  07 00 07            81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  13  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::verifyBit(1, 0, 6)).toByteArray(),
        "7f 7f  02  14  07 00 07            81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  15  07 00               81"_hex,
    }, {
        Request::sendDcc(DccRequest::verifyBit(1, 0, 7)).toByteArray(),
        "7f 7f  02  16  07 00 07            81"_hex,
    }, {
        Request::sendDcc(DccRequest::reset(5)).toByteArray(),
        "7f 7f  02  17  07 00               81"_hex,
    }, {
        Request::powerOff().toByteArray(),
        "7f 7f  02  18  05 00               81"_hex,
    }
};

const QList<KnownSequence> s_programmerInfo = {
    {
        "7f 7f  01  00  00                  81"_hex,
        "7f 7f  02  00  01 00               81"_hex, // cmd 0x00 -> 0x0001
    }, {
        "7f 7f  01  01  01                  81"_hex,
        "7f 7f  02  01  05 00               81"_hex, // cmd 0x01 -> 0x0500
    }, {
        "7f 7f  01  02  02 00               81"_hex,
        "7f 7f  02  02  05 00  97 00 00 00  81"_hex, // field #0 (ManId): 0x97
    }, {
        "7f 7f  01  03  02 01               81"_hex,
        "7f 7f  02  03  05 00  57 00 00 01  81"_hex, // field #0 (ProId): 0x57
    }, {
        "7f 7f  01  04  02 02               81"_hex,
        "7f 7f  02  04  05 00  ff ff ff ff  81"_hex, // field #2: 0xffffffff
    }, {
        "7f 7f  01  05  02 03               81"_hex,
        "7f 7f  02  05  05 00  ff ff ff ff  81"_hex, // field #3: 0xffffffff
    }, {
        "7f 7f  01  06  02 04               81"_hex,
        "7f 7f  02  06  05 00  5c 00 01 00  81"_hex, // BCode: 0.1.92
    }, {
        "7f 7f  01  07  02 05               81"_hex,
        "7f 7f  02  07  05 00  9b 3a e7 18  81"_hex, // BDate: 28.03.2013 (0x18e73a9b seconds since 2000-01-01T00:00)
    }, {
        "7f 7f  01  08  02 06               81"_hex,
        "7f 7f  02  08  05 00  86 00 01 00  81"_hex, // ACode: 0.1.134
    }, {
        "7f 7f  01  09  02 07               81"_hex,
        "7f 7f  02  09  05 00  f8 0c 48 20  81"_hex, // ADate: 28.02.2017 (0x20480cf8 seconds since 2000-01-01T00:00)
    }, {
        "7f 7f  01  0a  02 08               81"_hex,
        "7f 7f  02  0a  05 00  00 00 00 00  81"_hex, // field #8: 0x00000000
    }, {
        "7f 7f  01  0b  01                  81"_hex,
        "7f 7f  02  0b  05 00               81"_hex,
    }, {
        "7f 7f  01  0c  02 00               81"_hex,
        "7f 7f  02  0c  05 00  97 00 00 00  81"_hex, // ManId: 0x97
    }, {
        "7f 7f  01  0d  02 01               81"_hex,
        "7f 7f  02  0d  05 00  57 00 00 01  81"_hex, // ProId: 0x57
    }, {
        "7f 7f  01  0e  02 02               81"_hex,
        "7f 7f  02  0e  05 00  ff ff ff ff  81"_hex, // field #2: 0xffffffff
    }, {
        "7f 7f  01  0f  02 03               81"_hex,
        "7f 7f  02  0f  05 00  ff ff ff ff  81"_hex, // field #3: 0xffffffff
    }, {
        "7f 7f  01  10  02 04               81"_hex,
        "7f 7f  02  10  05 00  5c 00 01 00  81"_hex, // BCode: 0.1.92
    }, {
        "7f 7f  01  11  02 05               81"_hex,
        "7f 7f  02  11  05 00  9b 3a e7 18  81"_hex, // BDate: 28.03.2013 (0x18e73a9b seconds since 2000-01-01T00:00)
    }, {
        "7f 7f  01  12  02 06               81"_hex,
        "7f 7f  02  12  05 00  86 00 01 00  81"_hex, // ACode: 0.1.134
    }, {
        "7f 7f  01  13  02 07               81"_hex,
        "7f 7f  02  13  05 00  f8 0c 48 20  81"_hex, // ADate: 28.02.2017 (0x20480cf8 seconds since 2000-01-01T00:00)
    }, {
        "7f 7f  01  14  02 08               81"_hex,
        "7f 7f  02  14  05 00  00 00 00 00  81"_hex, // field #8: 0x00000000
    }
};

} // namespace

class Application
        : public core::StaticInit<Application>
        , public QApplication
{
public:
    using QApplication::QApplication;
    static void staticConstructor();

    int run();

private:
    int tryConnect();

    QList<KnownSequence> m_currentSequence;
};

void Application::staticConstructor()
{
    setApplicationName("ESU Programmer"_L1);
    setOrganizationDomain("taschenorakel.de"_L1);
}

int Application::run()
{
    /*
    if (true || arguments().contains("try-connect"_L1))
        return tryConnect();

    const QStringList testFileNames = {
//        ":/taschenorakel.de/tests/readprogrammerinfo.txt"_L1,
//        ":/taschenorakel.de/tests/readdecoderinfo.txt"_L1,
        ":/taschenorakel.de/tests/readvariable.txt"_L1,
//        ":/taschenorakel.de/tests/writevariable.txt"_L1,
//        ":/taschenorakel.de/tests/drive.txt"_L1,
    };

    for (const auto &fileName: testFileNames) {
        qInfo() << fileName;

        if (!tryParse(fileName)) {
            qWarning("Parsing failed");
            return EXIT_FAILURE;
        }

        qInfo() << "done" << Qt::endl;
        QThread::sleep(1);
    }

    qInfo() << Request::powerOn({PowerMode::PowerOff, 0});
    qInfo() << Request::powerOn({PowerMode::PowerOn, 45});
    qInfo() << Request::powerOn({PowerMode::Programming, 40});
    qInfo() << Request::powerOn({PowerMode::Programming, 32});
    qInfo() << Request::powerOff();

    QThread::sleep(1);

    return EXIT_SUCCESS;
    */
    const auto window = new MainWindow;
    window->show();

    return exec();
}

int Application::tryConnect()
{
    for (const auto &info: QSerialPortInfo::availablePorts())
        qInfo() << info.portName() << info.description() << info.manufacturer() << info.standardBaudRates();

    const auto serialPort = new QSerialPort{"COM5"_L1, this};

    connect(serialPort, &QSerialPort::readyRead, this, [this, serialPort] {
        const auto nextChunk = serialPort->read(64);
        static QByteArray buffer;
        buffer += nextChunk;

        if (buffer.isEmpty() || static_cast<quint8>(buffer.back()) != 0x81)
            return;

        qInfo() << "<R" << buffer.toHex(' ');

        if (buffer == m_currentSequence.first().response) {
            m_currentSequence.takeFirst();
            buffer.clear();

            if (m_currentSequence.isEmpty()) {
                exit(EXIT_SUCCESS);
            } else {
                qInfo() << ">W" << m_currentSequence.first().request.toHex(' ');
                serialPort->write(m_currentSequence.first().request);
            }
        } else {
            qWarning("Unexpected response, aborting");
            exit(EXIT_FAILURE);
        }
    });

    if (!serialPort->setBaudRate(115200)) {
        qCWarning(lcProgrammer, "%ls", qUtf16Printable(serialPort->errorString()));
        return EXIT_FAILURE;
    }

    if (!serialPort->setFlowControl(QSerialPort::HardwareControl)) {
        qCWarning(lcProgrammer, "%ls", qUtf16Printable(serialPort->errorString()));
        return EXIT_FAILURE;
    }

    qInfo() << serialPort->stopBits();
    qInfo() << serialPort->parity();
    qInfo() << serialPort->dataBits();
    qInfo() << serialPort->flowControl();

    if (!serialPort->open(QSerialPort::ReadWrite)) {
        qCWarning(lcProgrammer, "%ls", qUtf16Printable(serialPort->errorString()));
        return EXIT_FAILURE;
    }

    if (!serialPort->setDataTerminalReady(false))  {
        qCWarning(lcProgrammer, "%ls", qUtf16Printable(serialPort->errorString()));
        return EXIT_FAILURE;
    }

    m_currentSequence = s_readCV;
//    m_currentSequence = s_ledPlayground;
//    m_currentSequence = s_programmerInfo;

    qInfo() << ">W" << m_currentSequence.first().request.toHex(' ');
    serialPort->write(m_currentSequence.first().request);

    return exec();
}

} // namespace esu::programmer

int main(int argc, char *argv[])
{
    return lmrs::programmer::Application{argc, argv}.run();
}
