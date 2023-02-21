#include "localization.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/userliterals.h>

#include <QCoreApplication>
#include <QDirIterator>
#include <QTranslator>

namespace lmrs::core {

namespace {

struct Localization
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    static auto translate(QTranslator *t, const char *s) { return t->translate(staticMetaObject.className(), s); }
    static auto englishLanguageName(QTranslator *t) { return translate(t, QT_TR_NOOP("LANGUAGE_NAME_ENGLISH")); }
    static auto nativeLanguageName(QTranslator *t) { return translate(t, QT_TR_NOOP("LANGUAGE_NAME_NATIVE")); }
};

LMRS_CORE_DEFINE_LOGGER(Localization);

} // namespace

void installTranslations(QCoreApplication *app)
{
    if (app == nullptr) {
        installTranslations(QCoreApplication::instance());
        return;
    }

    for (auto it = QDirIterator(":/taschenorakel.de/lmrs"_L1, {"*.qm"_L1},
                                QDir::Files, QDirIterator::Subdirectories); it.hasNext(); ) {
        const auto file = it.nextFileInfo();
        auto translator = std::make_unique<QTranslator>(app);

        if (!translator->load(file.filePath())) {
            qCWarning(logger(), "Could not load translation froms \"%ls\"", qUtf16Printable(file.filePath()));
            continue;
        }

        // FIXME: do more sophisticated matching
        if (QLocale{}.language() == QLocale{translator->language()}.language()) {
            qCDebug(logger(), "Adding translations for %ls from \"%ls\"",
                   qUtf16Printable(translator->language()), qUtf16Printable(file.filePath()));

            app->installTranslator(translator.release());
        }
    }
}

} // namespace lmrs::core

#include "localization.moc"
