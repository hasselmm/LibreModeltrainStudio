#ifndef LMRS_ESU_LP2STREAM_H
#define LMRS_ESU_LP2STREAM_H

#include "lp2message.h"

#include <lmrs/core/framestream.h>

namespace lmrs::esu::lp2 {

using FrameFormat = core::StaticFrameFormat<0x7f, 0x81, 0x80>;

static_assert(FrameFormat::start()       == 0x7f);
static_assert(FrameFormat::startLength() ==    2);
static_assert(FrameFormat::stop()        == 0x81);
static_assert(FrameFormat::stopLength()  ==    1);
static_assert(FrameFormat::escape()      == 0x80);
static_assert(FrameFormat::mask()        == 0x00);

using StreamReader = core::GenericFrameStreamReader<FrameFormat, Message::HeaderSize>;
using StreamWriter = core::GenericFrameStreamWriter<FrameFormat>;

//class StreamWriter : public core::FrameStreamWriter
//{
//public:
//    StreamWriter(QIODevice *device = nullptr) : core::FrameStreamWriter{FrameFormat::instance(), device} {}
//};

} // namespace lmrs::esu::lp2

#endif // LMRS_ESU_LP2STREAM_H
