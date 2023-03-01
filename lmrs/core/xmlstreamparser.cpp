#include "xmlstreamparser.h"

#include "algorithms.h"
#include "logging.h"

#include <QLoggingCategory>

namespace lmrs::core::xml {

bool AbstractStreamParser::parse()
{
    for (m_stack = {{{initalState(), m_reader.name().toString()}}}; !m_stack.empty(); m_stack.pop()) {
        while (m_reader.readNextStartElement()) {
            if (!(this->*m_stack.back().handler)()) {
                qCWarning(m_logger, "Unexpected child <%ls> for element <%ls> at line %d:",
                          printable(m_reader.name().toString()), printable(m_stack.back().elementName),
                          static_cast<int>(m_reader.lineNumber()));

                m_reader.skipCurrentElement();
            }
        }
    }

    return m_reader.atEnd();
}

bool AbstractStreamParser::skipUnimplementedElement()
{
    m_reader.skipCurrentElement();
    return true;
}

bool AbstractStreamParser::enterState(StateHandler nextState)
{
    m_stack.emplaceBack(nextState, m_reader.name().toString());
    return true;
}

} // namespace lmrs::core::xml
