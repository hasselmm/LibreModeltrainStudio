#ifndef LMRS_GUI_L10N_LOCALIZATION_H
#define LMRS_GUI_L10N_LOCALIZATION_H

#include <lmrs/core/localization.h>

#include <QActionGroup>

namespace lmrs::gui::l10n {

class Action : public QAction
{
    Q_OBJECT

public:
    explicit Action(QObject *parent = nullptr);
    explicit Action(core::l10n::String text, QObject *parent = nullptr);
    explicit Action(core::l10n::String text, QKeySequence shortcut, QObject *parent = nullptr);
    explicit Action(core::l10n::String text, core::l10n::String toolTip, QObject *parent = nullptr);
    explicit Action(core::l10n::String text, core::l10n::String toolTip, QKeySequence shortcut,
                    QObject *parent = nullptr);

    explicit Action(QIcon icon, core::l10n::String text, QObject *parent = nullptr);
    explicit Action(QIcon icon, core::l10n::String text, core::l10n::String toolTip, QObject *parent = nullptr);
    explicit Action(QIcon icon, core::l10n::String text, core::l10n::String toolTip, QKeySequence shortcut,
                    QObject *parent = nullptr);

    template <class Receiver, typename Slot>
    explicit Action(Receiver *receiver, Slot slot)
        : Action{receiver}
    { connect(receiver, slot); }

    template <class Receiver, typename Slot>
    explicit Action(core::l10n::String text,
                    Receiver *receiver, Slot slot)
        : Action{std::move(text), receiver} { connect(receiver, slot); }

    template <class Receiver, typename Slot>
    explicit Action(core::l10n::String text, core::l10n::String toolTip,
                    QKeySequence shortcut, Receiver *receiver, Slot slot)
        : Action{std::move(text), std::move(toolTip), std::move(shortcut),
                 receiver} { connect(receiver, slot); }

    template <class Receiver, typename Slot>
    explicit Action(QIcon icon, core::l10n::String text, core::l10n::String toolTip,
                    Receiver *receiver, Slot slot)
        : Action{std::move(icon), std::move(text), std::move(toolTip),
                 receiver} { connect(receiver, slot); }

    template <class Receiver, typename Slot>
    explicit Action(QIcon icon, core::l10n::String text, core::l10n::String toolTip,
                    QKeySequence shortcut, Receiver *receiver, Slot slot)
        : Action{std::move(icon), std::move(text), std::move(toolTip),
                 std::move(shortcut), receiver} { connect(receiver, slot); }

    template <class Receiver, typename Slot>
    explicit Action(core::l10n::String text, QKeySequence shortcut, Receiver *receiver, Slot slot)
        : Action{std::move(text), std::move(shortcut), receiver} { connect(receiver, slot); }

    void setText(core::l10n::String newText);
    core::l10n::String text() const;

    void setToolTip(core::l10n::String newToolTip);
    core::l10n::String toolTip() const;

protected:
    bool eventFilter(QObject *target, QEvent *event) override;

private:
    template <class Receiver, class ReceiverOrBase, typename... Args>
    void connect(Receiver *receiver, void (ReceiverOrBase::*slot)())
    {
        static_assert(std::is_base_of_v<ReceiverOrBase, Receiver>);
        QAction::connect(this, &Action::triggered, receiver, slot);
    }

    template <class Receiver, typename... Args>
    void connect(Receiver *receiver, std::function<void()> slot)
    {
        QAction::connect(this, &Action::triggered, receiver, std::move(slot));
    }

    template <class Receiver, class ReceiverOrBase, typename... Args>
    void connect(Receiver *receiver, void (ReceiverOrBase::*slot)(bool))
    {
        static_assert(std::is_base_of_v<ReceiverOrBase, Receiver>);
        QAction::connect(this, &Action::triggered, receiver, slot);
        setCheckable(true);
    }

    core::l10n::String m_text;
    core::l10n::String m_toolTip;
};

class ActionGroup : public QActionGroup
{
    Q_OBJECT

public:
    using QActionGroup::QActionGroup;

    Action *addAction(Action *action);
    Action *addAction(auto... args) { return addAction(new Action{args..., this}); }
};

} // namespace lmrs::gui::l10n

namespace lmrs::core::l10n { using namespace gui::l10n; }

#endif // LMRS_GUI_L10N_LOCALIZATION_H
