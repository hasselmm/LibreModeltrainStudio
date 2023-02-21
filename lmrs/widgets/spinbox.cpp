#include "spinbox.h"

#include <lmrs/core/userliterals.h>
#include <lmrs/gui/symbolfonts.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>

namespace lmrs::widgets {

namespace {

constexpr auto binaryDigitCount(quint64 value)
{
    if (value < 0x10000) {
        if (value < 0x100)
            return 8;

        return 16;
    } else {
        if (value < 0x100000000)
            return 32;

        return 64;
    }
}
constexpr auto binaryDigitCount(int value)
{
    return binaryDigitCount(static_cast<quint64>(value));
}

constexpr auto binaryTextLength(quint64 value)
{
    const auto digits = binaryDigitCount(value);
    return digits + digits/4 - 1;
}

constexpr auto binaryTextLength(int value)
{
    return binaryTextLength(static_cast<quint64>(value));
}

auto binaryInputMask(quint64 value)
{
    auto inputMask = QString{binaryTextLength(value), Qt::Uninitialized};

    for (auto i = 0; i < inputMask.length(); ++i)
        inputMask[i] = (i % 5 == 4 ? ' '_L1 : 'B'_L1);

    return inputMask;
}

auto binaryInputMask(int value)
{
    return binaryInputMask(static_cast<quint64>(value));
}

static_assert(binaryDigitCount(255) == 8);
static_assert(binaryDigitCount(256) == 16);
static_assert(binaryDigitCount(65535) == 16);
static_assert(binaryDigitCount(65536) == 32);

static_assert(binaryTextLength(255) == 9);
static_assert(binaryTextLength(65535) == 19);

}

MultiBaseSpinBox::MultiBaseSpinBox(QWidget *parent)
    : QSpinBox{parent}
    , m_baseActionGroup{new QActionGroup{lineEdit()}}
{
    m_baseActionGroup->setExclusive(true);

    const auto createBaseAction = [this](int base, QString iconName, QString text) {
        using namespace gui::symbolfonts;

        const auto fontFamily = lineEdit()->font().family();
        const auto checkedIcon = icon("lmrs:checked:"_L1 + iconName, fontFamily, Style::Selected, 0.5, iconName);
        const auto uncheckedIcon = icon("lmrs:unchecked:"_L1 + iconName, fontFamily, Style::Normal, 0.5, iconName);

        const auto action = new QAction{std::move(text), m_baseActionGroup};

        action->setCheckable(true);
        action->setChecked(displayIntegerBase() == base); // call before connect to avoid setting icons twice for active icon
        action->setData(QVariant::fromValue(QList{uncheckedIcon, checkedIcon}));

        lineEdit()->addAction(action, QLineEdit::ActionPosition::LeadingPosition);

        const auto onToggled = [=, this](bool checked) {
            if (checked) {
                if (base == 2) {
                    setDisplayIntegerBase(2);
                    lineEdit()->setInputMask(binaryInputMask(maximum()));
                } else {
                    lineEdit()->setInputMask({});
                    setDisplayIntegerBase(base);
                }
            }

            action->setIcon(checked ? checkedIcon : uncheckedIcon);
        };

        connect(action, &QAction::toggled, this, onToggled);
        onToggled(action->isChecked());
    };

    createBaseAction(2,  tr("BIN"), tr("&Binary numbers"));
    createBaseAction(10, tr("DEC"), tr("&Decimal numbers"));
    createBaseAction(16, tr("HEX"), tr("&Hexadecimal numbers"));
}

QValidator::State MultiBaseSpinBox::validate(QString &input, int &pos) const
{
    if (displayIntegerBase() != 2)
        return QSpinBox::validate(input, pos);

    // FIXME: do we really need this with inputMask() set?
    for (int i = 0; i < input.length(); ++i) {
        if (i % 5 == 4) {
            if (input[i] != ' '_L1)
                return QValidator::Invalid;
        } else {
            if (input[i] != '0'_L1 && input[i] != '1'_L1)
                return QValidator::Invalid;
        }
    }

    if (input.length() != binaryTextLength(maximum()))
        return QValidator::Intermediate;

    return QValidator::Acceptable;
}

int MultiBaseSpinBox::valueFromText(const QString &text) const
{
    return QSpinBox::valueFromText(QString{text}.replace(' '_L1, QString{}));
}

QString MultiBaseSpinBox::textFromValue(int value) const
{
    if (displayIntegerBase() != 2)
        return QSpinBox::textFromValue(value);

    if (binaryTextLength(maximum()) != lineEdit()->inputMask().length())
        QTimer::singleShot(0, this, &MultiBaseSpinBox::updateInputMask);

    const auto digits = binaryDigitCount(maximum());
    auto text = QString::number(value, 2).rightJustified(digits, '0'_L1);

    // separate into groups of four bits
    for (auto i = text.length() - 4; i > 0; i -= 4)
        text.insert(i, ' '_L1);

    return text;
}

void MultiBaseSpinBox::contextMenuEvent(QContextMenuEvent *event)
{
    const auto menu = lineEdit()->createStandardContextMenu();

    if (!menu)
        return;

    connect(menu, &QMenu::aboutToHide, menu, &QMenu::deleteLater);

    // redirect select-all action to spinbox version of select-all
    // FIXME: find a more robust way to do this
    if (const auto selectAllAction = menu->actions().constLast()) {
        selectAllAction->disconnect(lineEdit());
        connect(selectAllAction, &QAction::triggered, this, &QSpinBox::selectAll);
    }

    menu->addSeparator();

    // restore step-up and step-down actions of the spinbox
    // FIXME: propose Qt patch for proper extension point
    if (const auto action = menu->addAction(tr("Step &up"))) {
        connect(action, &QAction::triggered, this, &QSpinBox::stepUp);
        action->setEnabled(stepEnabled() & StepUpEnabled);
    }

    if (const auto action = menu->addAction(tr("Step &down"))) {
        connect(action, &QAction::triggered, this, &QSpinBox::stepDown);
        action->setEnabled(stepEnabled() & StepDownEnabled);
    }

    menu->addSeparator();

    // add actions to change numeric base from context menu
    for (const auto baseAction: m_baseActionGroup->actions()) {
        const auto action = menu->addAction(baseAction->text());
        connect(action, &QAction::triggered, baseAction, &QAction::trigger);

        action->setCheckable(true);
        action->setChecked(baseAction->isChecked());
        action->setIcon(baseAction->data().value<QList<QIcon>>().constFirst());
    }

    menu->popup(event->reason() != QContextMenuEvent::Mouse
            ? mapToGlobal(QPoint{event->pos().x(), 0}) + QPoint{width() / 2, height() / 2}
            : event->globalPos());

    event->accept();
}

void MultiBaseSpinBox::updateInputMask()
{
    if (displayIntegerBase() == 2
            && lineEdit()->inputMask().length() != binaryTextLength(maximum())) {
        lineEdit()->setInputMask(binaryInputMask(maximum()));
        setValue(value());
    }
}

} // namespace lmrs::widgets
