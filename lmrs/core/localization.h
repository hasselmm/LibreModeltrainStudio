#ifndef LMRS_CORE_LOCALIZATION_H
#define LMRS_CORE_LOCALIZATION_H

#include "typetraits.h"

#include <QCoreApplication>
#include <QLocale>

#define LMRS_TR(SourceText, ...) \
    ::lmrs::core::l10n::String{LMRS_STATIC_METAOBJECT, (SourceText), __VA_ARGS__}

namespace lmrs::core::l10n {

template <class T>
concept Translatable = requires(T &translatable) { translatable.retranslate(); };

class String
{
public:
    using Filter = QString (*)(QString &);

    constexpr String() noexcept = default;
    constexpr String(const QMetaObject *metaObject, const char *sourceText,
           const char *disambiguation = nullptr, int n = -1,
           Filter filter = {}) noexcept
        : m_metaObject{metaObject}
        , m_sourceText{sourceText}
        , m_disambiguation{disambiguation}
        , m_number{n}
        , m_filter{filter}
    {}

    constexpr String(const String &other, Filter filter) noexcept
        : String{other.m_metaObject, other.m_sourceText, other.m_disambiguation, other.m_number, filter}
    {}

    [[nodiscard]] String filtered(Filter filter) const noexcept { return {*this, std::move(filter)}; }

    [[nodiscard]] constexpr auto metaObject() const noexcept { return m_metaObject; }
    [[nodiscard]] constexpr auto sourceText() const noexcept { return m_sourceText; }

public:
    [[nodiscard]] constexpr bool operator==(const String &rhs) const noexcept = default;

    [[nodiscard]] explicit operator bool() const;
    [[nodiscard]] QString toString() const;

private:
    const QMetaObject *m_metaObject = nullptr;
    const char *m_sourceText = nullptr;
    const char *m_disambiguation = nullptr;
    int m_number = 0;
    Filter m_filter = nullptr;
};

template<class T>
class Facade : public T
{
public:
    explicit Facade(String text, auto... args)
        : T{std::move(args)...}
        , m_text(std::move(text))
    {
        qApp->installEventFilter(this);
        applyText();
    }

protected:
    void applyText()
    {
        if constexpr (requires() { T::text(); })
            T::setText(m_text.toString());
        else if constexpr (requires() { T::title(); })
            T::setTitle(m_text.toString());
    }

    bool eventFilter(QObject *target, QEvent *event) override
    {
        if (event->type() == QEvent::LanguageChange && target == qApp)
            applyText();

        return false; // proceed with processing the event, do not stop it
    }

private:
    String m_text;
};

struct Language
{
    using Identifier = QLocale::Language;

    Identifier id;
    QString englishName;
    QString nativeName;
};

class LanguageManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(lmrs::core::l10n::Language::Identifier currentLanguage
               READ currentLanguage WRITE setCurrentLanguage
               NOTIFY currentLanguageChanged FINAL)

public:
    explicit LanguageManager(QObject *parent = nullptr);

    void setCurrentLanguage(Language::Identifier newLanguage);
    Language::Identifier currentLanguage() const;

    QList<Language> availableLanguages() const;

    template<Translatable T>
    T *create(QObject *parent = nullptr) const
    {
        auto translatable = new T{parent};
        connect(this, &LanguageManager::currentLanguageChanged, translatable, &T::retranslate);
        return translatable;
    }

signals:
    void currentLanguageChanged(lmrs::core::l10n::Language::Identifier currentLanguage);

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::core::l10n

namespace lmrs { namespace l10n = core::l10n; }

#endif // LMRS_CORE_LOCALIZATION_H
