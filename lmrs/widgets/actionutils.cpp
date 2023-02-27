#include "actionutils.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/typetraits.h>

#include <lmrs/gui/localization.h>

#include <QToolBar>
#include <QToolButton>

namespace lmrs::widgets {

namespace {

struct ActionUtils
{
    Q_GADGET
};

auto pointerToTriggerSignal(QAbstractButton *)
{
    return &QAbstractButton::clicked;
}

auto pointerToTriggerSignal(QAction *)
{
    return &QAction::triggered;
}

template<class TargetType>
TargetType *bindAction(TargetType *target, QAction *source, BindActionOptions options)
{
    auto updateTarget = [options, source, target] {
        if (!options.testFlag(BindActionOption::IgnoreIcon))
            target->setIcon(source->icon());
        if (!options.testFlag(BindActionOption::IgnoreText))
            target->setText(source->text());
        if (!options.testFlag(BindActionOption::IgnoreToolTip))
            target->setToolTip(source->toolTip());
    };

    auto updateVisible = [source, target] {
        target->setVisible(source->isVisible());
    };

    QAction::connect(source, &QAction::changed, target, updateTarget);
    QAction::connect(source, &QAction::checkableChanged, target, &TargetType::setCheckable);
    QAction::connect(source, &QAction::toggled, target, &TargetType::setChecked);
    QAction::connect(source, &QAction::enabledChanged, target, &TargetType::setEnabled);
    QAction::connect(source, &QAction::visibleChanged, target, updateVisible);

    QAction::connect(target, pointerToTriggerSignal(target), source, &QAction::trigger);

    updateTarget();
    return target;
}

} // namespace

QAbstractButton *bindAction(QAbstractButton *target, QAction *action, BindActionOptions options)
{
    return bindAction<QAbstractButton>(target, action, options);
}

QAction *bindAction(QAction *target, QAction *action, BindActionOptions options)
{
    return bindAction<QAction>(target, action, options);
}

void forceMenuButtonMode(QToolBar *toolBar, QAction *action)
{
    if (const auto button = core::checked_cast<QToolButton *>(toolBar->widgetForAction(action))) {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    } else {
        qCWarning(core::logger<ActionUtils>, "Could not find action \"%ls\" in toolbar",
                  qUtf16Printable(action->text()));
    }
}

QAction *makeCheckable(QAction *action)
{
    action->setCheckable(true);
    return action;
}

} // namespace lmrs::widgets

#include "actionutils.moc"
