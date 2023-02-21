#ifndef LMRS_CORE_XMLSTREAMPARSER_H
#define LMRS_CORE_XMLSTREAMPARSER_H

#include <QStack>
#include <QXmlStreamReader>

class QLoggingCategory;

namespace lmrs::core::xml {

class AbstractStreamParser
{
public:
    using LoggingFactory = const QLoggingCategory &(*)();
    using StateHandler = bool (AbstractStreamParser::*)();

    template <typename... Args>
    explicit AbstractStreamParser(LoggingFactory logger, Args... args)
        : m_logger{logger}
        , m_reader{args...}
    {}

    bool parse();

protected:
    virtual StateHandler initalState() = 0;

    template <class T>
    bool enterState(bool (T::*handler)()) { return enterState(static_cast<StateHandler>(handler)); }
    bool enterState(StateHandler nextState);
    bool skipUnimplementedElement();

    auto &xml() { return m_reader; }

private:
    struct StackFrame
    {
        StackFrame(StateHandler handler, QString elementName) noexcept
            : handler{std::move(handler)}
            , elementName{std::move(elementName)}
        {}

        StateHandler handler;
        QString elementName;
    };

    const LoggingFactory m_logger;
    QStack<StackFrame> m_stack;
    QXmlStreamReader m_reader;
};

template<class T>
class StreamParser : public AbstractStreamParser
{
public:
    using AbstractStreamParser::AbstractStreamParser;

protected:
    StateHandler initalState() override { return static_cast<StateHandler>(&T::readDocumentRoot); }
};

} // namespace lmrs::core::xml

#endif // LMRS_CORE_XMLSTREAMPARSER_H
