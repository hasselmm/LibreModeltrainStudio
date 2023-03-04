#include "userliterals.h"

#include <QRegularExpression>
#include <QUrl>

namespace lmrs::core::literals {

static_assert("123"_L1.size() == 3);
static_assert("*.zip"_wildcard.pattern.size() == 5);

namespace {

auto toByteArray(QLatin1StringView s)
{
    return QByteArray{s.constData(), s.size()};
}

constexpr auto toPatternOption(Qt::CaseSensitivity cs)
{
    switch (cs) {
    case Qt::CaseSensitive:
        return QRegularExpression::NoPatternOption;
    case Qt::CaseInsensitive:
        return QRegularExpression::CaseInsensitiveOption;
    }

    return QRegularExpression::NoPatternOption;
}

} // namespace

QUrl operator ""_url(const char *str, size_t len)
{
    return QUrl{QLatin1StringView{str, static_cast<int>(len)}};
}

QRegularExpression operator ""_regex(const char *str, size_t len)
{
    return QRegularExpression{QLatin1StringView{str, static_cast<int>(len)}};
}

template<class Tag>
QRegularExpressionMatch internal::RegularExpressionLiteralBase<Tag>::match(QString subject) const noexcept
{
    return compile().match(std::move(subject));
}

template<>
QRegularExpression internal::RegularExpressionLiteralBase
<struct RegularExpressionLiteralTag>::compile(Qt::CaseSensitivity cs) const noexcept
{
    return QRegularExpression{pattern.toString(), toPatternOption(cs)};
}

template<>
QRegularExpression internal::RegularExpressionLiteralBase
<struct WildcardLiteralTag>::compile(Qt::CaseSensitivity cs) const noexcept
{
    return QRegularExpression::fromWildcard(pattern.toString(), cs);
}

template<class Tag>
internal::RegularExpressionLiteralBase<Tag>::operator QRegularExpression() const noexcept
{
    return compile();
}

template<class Tag>
size_t internal::RegularExpressionLiteralBase<Tag>::qHash(size_t seed) const noexcept
{
    return qHashBits(pattern.constData(), static_cast<size_t>(pattern.size()), seed);
}

RegularExpressionLiteral RegularExpressionLiteral::operator+(RegularExpressionLiteral rhs) const noexcept
{
    return {QLatin1StringView{toByteArray(pattern) + toByteArray(std::move(rhs.pattern))}};
}

template struct internal::RegularExpressionLiteralBase<struct RegularExpressionLiteralTag>;
template struct internal::RegularExpressionLiteralBase<struct WildcardLiteralTag>;

} // namespace lmrs::core::literals
