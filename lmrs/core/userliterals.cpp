#include "userliterals.h"

#include <QUrl>

namespace lmrs::core::literals {

static_assert("123"_L1.size() == 3);

QUrl operator ""_url(const char *str, size_t len)
{
    return QUrl{QLatin1StringView{str, static_cast<int>(len)}};
}

} // namespace lmrs::core::literals
