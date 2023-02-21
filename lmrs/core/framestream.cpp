#include "framestream.h"

#include <QIODevice>
#include <QPointer>

namespace lmrs::core {

// =====================================================================================================================

QByteArray FrameFormat::startSequence() const noexcept
{
    return {startLength, static_cast<char>(start)};
}

QByteArray FrameFormat::stopSequence() const noexcept
{
    return {stopLength, static_cast<char>(stop)};
}

QByteArray FrameFormat::escaped(QByteArrayView data) const noexcept
{
    auto result = QByteArray{};

    result.reserve(escapedLength(data));
    result += startSequence();

    for (const auto ch: data) {
        if (escapeNeeded(ch)) {
            result += static_cast<char>(escape);
            result += escaped(ch);
        } else {
            result += ch;
        }
    }

    result += stopSequence();

    return result;
}

// =====================================================================================================================

class FrameStreamReader::Private
{
public:
    const FrameFormat format;
    QPointer<QIODevice> device;

    QByteArray buffer;
    QByteArray frame;

    bool readNext(int minimumSize);
};

// ---------------------------------------------------------------------------------------------------------------------

FrameStreamReader::FrameStreamReader(FrameFormat format, QByteArrayView data)
    : d{new Private{std::move(format), nullptr, QByteArray{}, QByteArray{}}}
{
    d->buffer.append(std::move(data));
}

FrameStreamReader::FrameStreamReader(FrameFormat format, QIODevice *device)
    : d{new Private{std::move(format), device, QByteArray{}, QByteArray{}}}
{}

FrameStreamReader::~FrameStreamReader()
{
    delete d.get();
}

void FrameStreamReader::setDevice(QIODevice *device)
{
    clear();
    d->device = device;
}

QIODevice *FrameStreamReader::device() const
{
    return d->device;
}

void FrameStreamReader::addData(QByteArrayView data)
{
    d->buffer.append(std::move(data));
}

void FrameStreamReader::clear()
{
    d->buffer.clear();
}

bool FrameStreamReader::isAtEnd() const
{
    if (d->device)
        return d->device->atEnd();

    return d->buffer.isEmpty();
}

bool FrameStreamReader::Private::readNext(int minimumSize)
{
    if (device) {
        if (const auto available = device->bytesAvailable(); available > 0)
            buffer.append(device->read(available));
    }

    frame.clear();

    for (auto offset = qsizetype{0}; offset < buffer.size(); ) {
        const auto start = buffer.indexOf(static_cast<char>(format.start), offset);

        if (start < 0) {
            buffer.clear(); // This buffer cannot contain any message. Therefore just cleanup already.
            return false;
        }

        const auto minimumFrameSize = format.startLength + minimumSize + format.stopLength;

        if (start + minimumFrameSize > buffer.size())
            return false; // This buffer is too short to contain a full message. Therefore abort for now.

        // Skip over start markers in case there are more than the expected number
        for (offset = start + 1; offset < buffer.size(); ++offset) {
             if (!format.isStartMarker(buffer[offset]))
                 break;
        }

        if (offset - start < format.startLength)
            continue; // Insufficent number of start markers. This is garbage data, skip it.

        auto nextFrame = QByteArray{};

        while (offset < buffer.size()) {
            const auto ch = buffer[offset++];

            if (format.isStopMarker(ch)) {
                buffer.remove(0, offset);               // Remove all data up to the end of this frame from buffer.
                std::exchange(frame, nextFrame);        // Next frame found. Store it.
                return true;                            // Report success.
            } else if (format.isStartMarker(ch)) {
                continue;                               // Invalid character found. Skip corrupted frame.
            } else if (!format.isEscapeMarker(ch)) {
                nextFrame.append(ch);                   // Regular byte found that doesn't need escaping.
            } else if (offset + 1 < buffer.size()) {
                nextFrame.append(format.unescaped(buffer[offset++]));  // Store escaped byte.
            } else {
                return false;                           // Buffer ends within escape sequence. Abort for now.
            }
        }
    }

    return false;
}

bool FrameStreamReader::readNext(int minimumSize)
{
    return d->readNext(minimumSize);
}

QByteArray FrameStreamReader::frame() const
{
    return d->frame;
}

qsizetype FrameStreamReader::bufferedBytes() const
{
    return d->buffer.size();
}

// =====================================================================================================================

class FrameStreamWriter::Private
{
public:
    const FrameFormat format;
    QPointer<QIODevice> device;
    QString errorString;

    bool writeFrame(QByteArrayView data);
};

// ---------------------------------------------------------------------------------------------------------------------

FrameStreamWriter::FrameStreamWriter(FrameFormat format, QIODevice *device)
    : d{new Private{std::move(format), device, QString{}}}
{}

FrameStreamWriter::~FrameStreamWriter()
{
    delete d.get();
}

void FrameStreamWriter::setDevice(QIODevice *device)
{
    d->device = device;
}

QIODevice *FrameStreamWriter::device() const
{
    return d->device;
}

bool FrameStreamWriter::Private::writeFrame(QByteArrayView data)
{
    if (device.isNull()) {
        errorString = tr("Cannot write without device attached");
        return false;
    }

    const auto frameSize = format.escapedLength(data);
    auto frame = QByteArray{frameSize, Qt::Uninitialized};
    auto it = frame.begin();

    for (auto i = 0; i < format.startLength; ++i)
        *it++ = static_cast<char>(format.start);

    for (const auto ch: data) {
        if (format.escapeNeeded(ch)) {
            *it++ = static_cast<char>(format.escape);
            *it++ = format.escaped(ch);
        } else {
            *it++ = ch;
        }
    }

    for (auto i = 0; i < format.stopLength; ++i)
        *it++ = static_cast<char>(format.stop);

    const auto bytesToWrite = frame.size();
    return device->write(std::move(frame)) == bytesToWrite;
}

bool FrameStreamWriter::writeFrame(QByteArrayView data)
{
    return d->writeFrame(std::move(data));
}

QString FrameStreamWriter::errorString() const
{
    if (!d->errorString.isEmpty())
        return d->errorString;
    if (d->device)
        return d->device->errorString();

    return {};
}

// =====================================================================================================================

} // namespace lmrs::core
