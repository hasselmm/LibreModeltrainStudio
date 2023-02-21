#include "audiopreview.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QBoxLayout>
#include <QBuffer>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QPushButton>

#include <QtEndian>

namespace {

namespace riff {

struct Tag
{
    char data[4] = {};

    bool operator==(const Tag &rhs) const { return memcmp(data, rhs.data, sizeof data) == 0; }
};

static constexpr auto RIFF = Tag{'R', 'I', 'F', 'F'};
static constexpr auto RIFX = Tag{'R', 'I', 'F', 'X'};

struct Chunk
{
    Tag     tag;
    quint32 size = {};
};

struct Header
{
    Chunk   descriptor;
    Tag     type;
};

class Decoder
{
public:
    bool isBigEndian() const { return m_header.descriptor.tag.data[3] == 'X'; }
    auto size() const { return m_header.descriptor.size; }
    auto type() const { return m_header.type; }

    bool open(QIODevice *device);
    qint64 findChunk(Tag tag);

    template<typename T>
    T convertEndian(T value) const;

    template<typename T>
    bool read(T *value);

private:
    QIODevice *m_device = {};
    Header m_header;
};

template<typename T>
T Decoder::convertEndian(T value) const
{
    if (isBigEndian())
        return qFromBigEndian(value);
    else
        return qFromLittleEndian(value);
}

template<typename T>
bool Decoder::read(T *value)
{
    if (m_device) {
        const auto size = m_device->read(reinterpret_cast<char *>(value), sizeof(T));
        return size == sizeof(T);
    }

    return false;
}

bool Decoder::open(QIODevice *device)
{
    if (m_device)
        return false;

    m_device = device;

    if (!read(&m_header))
        return false;

    if (m_header.descriptor.tag != RIFF
            && m_header.descriptor.tag != RIFX)
        return false;

    m_header.descriptor.size = convertEndian(m_header.descriptor.size);

    return true;
}

qint64 Decoder::findChunk(Tag tag)
{
    if (m_device && m_device->seek(sizeof m_header)) {
        const auto endOfFile = static_cast<qint64>(sizeof m_header.descriptor + m_header.descriptor.size);

        while (m_device->pos() < endOfFile) {
            Chunk descriptor;

            // fail if not sufficient bytes available for chunk descriptor
            if (!read(&descriptor))
                break;

            descriptor.size = convertEndian(descriptor.size);

            if (descriptor.tag == tag)
                return descriptor.size;

            // fail if not sufficient bytes available for chunk
            if (m_device->skip(descriptor.size) != descriptor.size)
                break;
        }
    }

    return -1;
}

} // namespace riff

namespace wave {

static constexpr auto TYPE = riff::Tag{'W', 'A', 'V', 'E'};
static constexpr auto FORMAT  = riff::Tag{'f', 'm', 't', ' '};
static constexpr auto DATA = riff::Tag{'d', 'a', 't', 'a'};

struct Header
{
    quint16 audioFormat;
    quint16 numChannels;
    quint32 sampleRate;
    quint32 byteRate;
    quint16 blockAlign;
    quint16 bitsPerSample;
};

} // namespace wave



} // namespace

class AudioPreview::Private
{
public:
    explicit Private(AudioPreview *q)
        : playButton{new QPushButton{tr("&Play"), q}}
        , stopButton{new QPushButton{tr("&Stop"), q}}
        , infoLabel{new QLabel{q}}
    {}

    static QString describeChannelCount(QAudioFormat format)
    {
        if (format.channelCount() == 1)
            return tr("mono");
        else if (format.channelCount() == 2)
            return tr("stereo");
        else
            return tr("%1 channels").arg(format.channelCount());
    }


    QPushButton *const playButton;
    QPushButton *const stopButton;
    QLabel *const infoLabel;

    QPointer<QAudioSink> output;
    QPointer<QBuffer> samples;
};

AudioPreview::AudioPreview(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    d->playButton->setCheckable(true);
    d->infoLabel->setAlignment(Qt::AlignCenter);

    const auto buttonBox = new QHBoxLayout{};
    buttonBox->addStretch(1);
    buttonBox->addWidget(d->playButton);
    buttonBox->addWidget(d->stopButton);
    buttonBox->addStretch(1);

    const auto vbox = new QVBoxLayout{this};
    vbox->addStretch(1);
    vbox->addLayout(buttonBox);
    vbox->addWidget(d->infoLabel);
    vbox->addStretch(1);

    connect(d->playButton, &QPushButton::clicked, this, &AudioPreview::play);
    connect(d->stopButton, &QPushButton::clicked, this, &AudioPreview::stop);
}

AudioPreview::~AudioPreview()
{
    delete d;
}

bool AudioPreview::load(QByteArray data)
{
    if (d->output)
        d->output->disconnect(this);

    delete d->output;
    delete d->samples;

    if (auto buffer = QBuffer{&data}; buffer.open(QBuffer::ReadOnly)) {
        if (riff::Decoder riff; riff.open(&buffer) && riff.type() == wave::TYPE) {
            wave::Header header;

            if (const auto headerSize = riff.findChunk(wave::FORMAT);
                    headerSize >= qint64(sizeof header) && riff.read(&header)) {
                auto format = QAudioFormat{};
//                format.setCodec("audio/pcm");

//                format.setByteOrder(riff.isBigEndian() ? QAudioFormat::BigEndian
//                                                       : QAudioFormat::LittleEndian);
                format.setSampleRate(riff.convertEndian(static_cast<int>(header.sampleRate)));
                format.setChannelCount(riff.convertEndian(header.numChannels));

                const auto bps = riff.convertEndian(header.bitsPerSample);
                format.setSampleFormat(bps == 8 ? QAudioFormat::UInt8 : QAudioFormat::Int16);
//                format.setSampleType(bps == 8 ? QAudioFormat::UnSignedInt : QAudioFormat::SignedInt);
//                format.setSampleSize(bps);

                if (const auto dataSize = riff.findChunk(wave::DATA); dataSize >= 0) {
                    d->samples = new QBuffer{this};
                    d->samples->setData(buffer.read(dataSize));

                    if (d->samples->open(QBuffer::ReadOnly)) {
                        d->output = new QAudioSink{format, this};

                        connect(d->output, &QAudioSink::stateChanged, [this](auto state) {
                            if (state == QAudio::ActiveState) {
                                d->playButton->setChecked(true);
                                emit started();
                            } else if (state == QAudio::IdleState
                                       || state == QAudio::StoppedState) {
                                d->playButton->setChecked(false);
                                emit finished();
                            }
                        });

                        const auto duration = static_cast<qreal>(d->samples->size())
                                / format.bytesPerFrame()
                                / format.sampleRate();
                        const auto locale = QLocale{};

                        d->infoLabel->setText(tr("%1 Hz, %2 bit, %3\nduration: %4 s").
                                              arg(locale.toString(format.sampleRate())).
                                              arg(0).//format.sampleSize()).
                                              arg(Private::describeChannelCount(format)).
                                              arg(locale.toString(duration, 'f', 1)));
                    } else {
                        delete d->samples;
                    }
                }
            }
        }
    }

    return d->output;
}

void AudioPreview::play()
{
    if (d->samples && d->output) {
        d->samples->seek(0);
        d->output->start(d->samples);
    }
}

void AudioPreview::stop()
{
    if (d->output)
        d->output->stop();
}
