#ifndef LMRS_CORE_FRAMESTREAM_H
#define LMRS_CORE_FRAMESTREAM_H

#include "memory.h"

#include <QObject>

namespace lmrs::core {

// =====================================================================================================================

struct FrameFormat
{
    quint8 start;
    quint8 stop;
    quint8 escape;
    quint8 mask;
    qsizetype startLength;
    qsizetype stopLength;

    constexpr auto isStartMarker(char ch) const noexcept
    {
        return static_cast<quint8>(ch) == start;
    }

    constexpr auto isStopMarker(char ch) const noexcept
    {
        return static_cast<quint8>(ch) == stop;
    }

    constexpr auto isEscapeMarker(char ch) const noexcept
    {
        return static_cast<quint8>(ch) == escape;
    }

    constexpr auto escapeNeeded(char ch) const noexcept
    {
        return isStartMarker(ch)
                || isStopMarker(ch)
                || isEscapeMarker(ch);
    }

    constexpr auto countIfEscapeNeeded(QByteArrayView data) const noexcept
    {
        return std::count_if(data.begin(), data.end(), [this](auto ch) {
            return escapeNeeded(ch);
        });
    }

    constexpr auto escaped(char ch) const noexcept
    {
        return static_cast<char>(static_cast<quint8>(ch) ^ mask);
    }

    constexpr auto unescaped(char ch) const noexcept
    {
        return escaped(ch);
    }

    constexpr auto escapedLength(QByteArrayView data) const noexcept
    {
        return startLength + data.size() + countIfEscapeNeeded(std::move(data)) + stopLength;
    }

    QByteArray startSequence() const noexcept;
    QByteArray stopSequence() const noexcept;

    QByteArray escaped(QByteArrayView data) const noexcept;
};

// ---------------------------------------------------------------------------------------------------------------------

template<quint8 StartMarker, quint8 StopMarker,
         quint8 EscapeMarker, quint8 EscapeMask = 0,
         qsizetype StartLength = 2, qsizetype StopLength = 1>
struct StaticFrameFormat
{
    static constexpr auto start() { return instance().start; }
    static constexpr auto stop() { return instance().stop; }
    static constexpr auto escape() { return instance().escape; }
    static constexpr auto mask() { return instance().mask; }
    static constexpr auto startLength() { return instance().startLength; }
    static constexpr auto stopLength() { return instance().stopLength; }

    static constexpr auto escapeNeeded(char ch) { return instance().escapeNeeded(ch); }
    static constexpr auto escapedCharCount(QByteArrayView data) { return instance().escapedCharCount(std::move(data)); }

    static constexpr auto escaped(char ch) { return instance().escaped(ch); }
    static constexpr auto escapedLength(QByteArrayView data) { return instance().escapedLength(std::move(data)); }

    static constexpr auto instance()
    {
        return FrameFormat{StartMarker, StopMarker, EscapeMarker, EscapeMask, StartLength, StopLength};
    }

    static auto escaped(QByteArrayView data) noexcept { return instance().escaped(std::move(data)); }
};

static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::start() == 0x7f);
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::stop() == 0x81);
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::escape() == 0x80);
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::mask() == 0x00);
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::startLength() == 2);
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::stopLength() == 1);
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::escapeNeeded('\x7f'));
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::escapeNeeded('\x80'));
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::escapeNeeded('\x81'));
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::escaped('\x80') == '\x80');
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::escapedLength("ABC") == 6);
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::escapedLength("ABCDEFG") == 10);
static_assert(StaticFrameFormat<0x7f, 0x81, 0x80>::escapedLength("A\x7e\x7f\x80\x81\x82G") == 13);

static_assert(StaticFrameFormat<0x01, 0x17, 0x10, 0x20>::instance().mask == 0x20);
static_assert(StaticFrameFormat<0x01, 0x17, 0x10, 0x20>::instance().escapeNeeded('\x10'));
static_assert(StaticFrameFormat<0x01, 0x17, 0x10, 0x20>::instance().escaped('\x10') == '\x30');

// ---------------------------------------------------------------------------------------------------------------------

template<typename T>
concept FrameFormatType = std::is_same_v<decltype(T::instance()), FrameFormat>;

// =====================================================================================================================

class FrameStreamReader
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    explicit FrameStreamReader(FrameFormat format, QByteArrayView data);
    explicit FrameStreamReader(FrameFormat format, QIODevice *device = nullptr);
    ~FrameStreamReader();

    void setDevice(QIODevice *device);
    QIODevice *device() const;

    void addData(QByteArrayView data);
    void clear();

    bool isAtEnd() const;
    bool readNext(int minimumSize = 0);

    QByteArray frame() const;   
    qsizetype bufferedBytes() const;

private:
    class Private;
    ConstPointer<Private> d;
};

// ---------------------------------------------------------------------------------------------------------------------

class FrameStreamWriter
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    explicit FrameStreamWriter(FrameFormat format, QIODevice *device = nullptr);
    ~FrameStreamWriter();

    QString errorString() const;

    void setDevice(QIODevice *device);
    QIODevice *device() const;

    bool writeFrame(QByteArrayView data);

private:
    class Private;
    ConstPointer<Private> d;
};

// =====================================================================================================================

template<FrameFormatType T, int MinimumFrameSize = 0>
class GenericFrameStreamReader : public FrameStreamReader
{
public:
    using FrameFormat = T;

    explicit GenericFrameStreamReader(QByteArrayView data)
        : FrameStreamReader{FrameFormat::instance(), std::move(data)} {}

    explicit GenericFrameStreamReader(QIODevice *device = nullptr)
        : FrameStreamReader{FrameFormat::instance(), device} {}

    bool readNext() { return FrameStreamReader::readNext(MinimumFrameSize); }
};

template<FrameFormatType T>
class GenericFrameStreamWriter : public core::FrameStreamWriter
{
public:
    using FrameFormat = T;

    GenericFrameStreamWriter(QIODevice *device = nullptr)
        : core::FrameStreamWriter{FrameFormat::instance(), device} {}
};

// =====================================================================================================================

} // namespace lmrs::core

#endif // LMRS_CORE_FRAMESTREAM_H
