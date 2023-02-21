#include "documentpreview.h"

#include <lmrs/core/userliterals.h>
#include <lmrs/core/typetraits.h>

#include <QBoxLayout>
#include <QDomDocument>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextStream>

#include <QDebug>

using namespace lmrs; // FIXME

namespace {

class SyntaxRule
{
public:
    SyntaxRule(QColor color, QString pattern);

    auto pattern() const { return m_pattern; }
    auto regularFormat() const { return m_regularFormat; }
    auto boldFormat() const { return m_boldFormat; }

private:
    QRegularExpression m_pattern;
    QTextCharFormat m_regularFormat;
    QTextCharFormat m_boldFormat;
};

class XmlHighlighter : public QSyntaxHighlighter
{
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

protected: // QSyntaxHighlighter interface
    void highlightBlock(const QString &text) override;
};

SyntaxRule::SyntaxRule(QColor color, QString pattern)
    : m_pattern{std::move(pattern)}
{
    m_regularFormat.setForeground(std::move(color));
    m_boldFormat.setFontWeight(QFont::Bold);
    m_boldFormat.merge(m_regularFormat);
}

void XmlHighlighter::highlightBlock(const QString &text)
{
    static const QList<SyntaxRule> syntaxRules =  {
        {Qt::darkYellow, R"((<\?\w+)[^>]*\?>)"_L1},         // processing instruction
        {Qt::darkMagenta, R"((<(?:\w+:)?\w+)[^>]*>)"_L1},   // element start
        {Qt::darkMagenta, R"(</(?:\w+:)?\w+\s*>)"_L1},      // element end
        {Qt::darkCyan, R"((?:\w+:)?\w+=)"_L1},              // attribute
        {Qt::darkGreen, R"("[^"]*"|'[^']*')"_L1},           // string
    };

    for (const auto &rule: syntaxRules) {
        for (auto it = rule.pattern().globalMatch(text); it.hasNext(); ) {
            const auto match = it.next();

            setFormat(static_cast<int>(match.capturedStart()),
                      static_cast<int>(match.capturedLength()),
                      rule.regularFormat());

            if (match.capturedLength(1) > 0) {
                setFormat(static_cast<int>(match.capturedStart(1)),
                          static_cast<int>(match.capturedLength(1)),
                          rule.boldFormat());
            }
        }
    }
}

} // namespace

class DocumentPreview::Private : public core::PrivateObject<DocumentPreview>
{
public:
    using PrivateObject::PrivateObject;

    QPlainTextEdit *const textView = new QPlainTextEdit{q()};
};

DocumentPreview::DocumentPreview(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    new XmlHighlighter{d->textView->document()};

    d->textView->setReadOnly(true);

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(d->textView);
}

DocumentPreview::~DocumentPreview()
{
    delete d;
}

bool DocumentPreview::load(QByteArray data)
{
    if (auto document = QDomDocument{}; document.setContent(std::move(data))) {
        d->textView->setPlainText(document.toString());
        return true;
    }

    return false;
}
