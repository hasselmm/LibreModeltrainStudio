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

auto triggered(QAbstractButton *)
{
    return &QAbstractButton::clicked;
}

auto triggered(QAction *)
{
    return &QAction::triggered;
}

template<class TargetType>
TargetType *bindAction(TargetType *target, QAction *source)
{
    auto updateTarget = [source, target] {
        target->setCheckable(source->isCheckable());
        target->setIcon(source->icon());
        target->setText(source->text());
        target->setToolTip(source->toolTip());
    };

    QAction::connect(target, triggered(target), source, &QAction::trigger);
    QAction::connect(source, &QAction::changed, target, updateTarget);

    updateTarget();
    return target;
}

} // namespace

QAbstractButton *bindAction(QAbstractButton *target, QAction *action)
{
    return bindAction<QAbstractButton>(target, action);
}

QAction *bindAction(QAction *target, QAction *action)
{
    return bindAction<QAction>(target, action);
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
