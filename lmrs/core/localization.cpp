#include "localization.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/userliterals.h>
#include <lmrs/core/typetraits.h>

#include <QCoreApplication>
#include <QDirIterator>
#include <QTranslator>

namespace lmrs::core::l10n {

String::operator bool() const
{
    return m_metaObject && m_sourceText;
}

QString String::toString() const
{
    if (m_metaObject && m_sourceText) {
        auto text = m_metaObject->tr(m_sourceText, m_disambiguation, m_number);

        if (m_filter)
            text = m_filter(text);

        return text;
    }

    return {};
}

class LanguageManager::Private : public core::PrivateObject<LanguageManager>
{
    Q_OBJECT

public:
    using PrivateObject::PrivateObject;

    QList<Language> availableLanguages;
    Language::Identifier currentLanguage;
    QHash<Language::Identifier, QList<QTranslator *>> translators;

    static auto translate(QTranslator *t, const char *s) { return t->translate(staticMetaObject.className(), s); }
    static auto englishLanguageName(QTranslator *t) { return translate(t, QT_TR_NOOP("LANGUAGE_NAME_ENGLISH")); }
    static auto nativeLanguageName(QTranslator *t) { return translate(t, QT_TR_NOOP("LANGUAGE_NAME_NATIVE")); }

    static Language languageFromTranslator(QTranslator *translator)
    {
        return {
            QLocale{translator->language()}.language(),
            englishLanguageName(translator),
            nativeLanguageName(translator),
        };
    }
};

LanguageManager::LanguageManager(QObject *parent)
    : QObject{parent}
    , d{new Private{this}}
{
    auto languagesFound = QHash<Language::Identifier, Language>{};

    for (auto it = QDirIterator(":/taschenorakel.de/lmrs"_L1, {"*.qm"_L1},
                                QDir::Files, QDirIterator::Subdirectories); it.hasNext(); ) {
        const auto file = it.nextFileInfo();
        auto translator = std::make_unique<QTranslator>(this);

        if (!translator->load(file.filePath())) {
            qCWarning(logger(this), "Could not load translation froms \"%ls\"", qUtf16Printable(file.filePath()));
            continue;
        }

        auto language = Private::languageFromTranslator(translator.get());
        d->translators[language.id] += translator.release();

        if (!language.englishName.isEmpty()
                && !language.nativeName.isEmpty())
            languagesFound.insert(language.id, std::move(language));
    }

    d->availableLanguages = languagesFound.values();

    setCurrentLanguage(QLocale{}.language());
}

void LanguageManager::setCurrentLanguage(Language::Identifier newLanguage)
{
    if (const auto oldLanguage = std::exchange(d->currentLanguage, newLanguage); oldLanguage != newLanguage) {
        for (auto translator: std::as_const(d->translators[oldLanguage]))
            qApp->removeTranslator(translator);
        for (auto translator: std::as_const(d->translators[newLanguage]))
            qApp->installTranslator(translator);
    }
}

Language::Identifier LanguageManager::currentLanguage() const
{
    return d->currentLanguage;
}

QList<Language> LanguageManager::availableLanguages() const
{
    return d->availableLanguages;
}

} // namespace lmrs::core::l10n

#include "localization.moc"
