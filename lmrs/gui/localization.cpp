#include "localization.h"

#include <QGuiApplication>
#include <QEvent>

namespace lmrs::gui::l10n {

Action::Action(QObject *parent)
    : QAction{parent}
{
    qApp->installEventFilter(this);
}

Action::Action(core::l10n::String text, QObject *parent)
    : Action(parent)
{
    setText(std::move(text));
}

Action::Action(core::l10n::String text, QKeySequence shortcut, QObject *parent)
    : Action{std::move(text), parent}
{
    setShortcut(std::move(shortcut));
}

Action::Action(core::l10n::String text, core::l10n::String toolTip, QKeySequence shortcut, QObject *parent)
    : Action{std::move(text), std::move(shortcut), parent}
{
    setToolTip(std::move(toolTip));
}

Action::Action(QIcon icon, core::l10n::String text, QObject *parent)
    : Action{std::move(text), parent}
{
    setIcon(std::move(icon));
}

Action::Action(QIcon icon, core::l10n::String text, core::l10n::String toolTip, QObject *parent)
    : Action{std::move(icon), std::move(text), parent}
{
    setToolTip(std::move(toolTip));
}

Action::Action(QIcon icon, core::l10n::String text, core::l10n::String toolTip, QKeySequence shortcut, QObject *parent)
    : Action{std::move(icon), std::move(text), std::move(toolTip), parent}
{
    setShortcut(std::move(shortcut));
}

void Action::setText(core::l10n::String newText)
{
    if (std::exchange(m_text, std::move(newText)) != m_text)
        QAction::setText(m_text.toString());
}

core::l10n::String Action::text() const
{
    return m_text;
}

void Action::setToolTip(core::l10n::String newToolTip)
{
    if (std::exchange(m_toolTip, std::move(newToolTip)) != m_toolTip)
        QAction::setToolTip(m_toolTip.toString());
}

core::l10n::String Action::toolTip() const
{
    return m_toolTip;
}

bool Action::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && target == qApp) {
        QAction::setToolTip(m_toolTip.toString());
        QAction::setText(m_text.toString());
    }

    return false; // proceed with processing the event, do not stop it
}

} // namespace lmrs::gui::l10n
