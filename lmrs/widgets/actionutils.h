#ifndef LMRS_WIDGETS_ACTIONUTILS_H
#define LMRS_WIDGETS_ACTIONUTILS_H

#include <concepts>

class QAbstractButton;
class QAction;
class QObject;
class QToolBar;

namespace lmrs::widgets {

QAbstractButton *bindAction(QAbstractButton *target, QAction *action);
QAction *bindAction(QAction *target, QAction *action);
QAction *makeCheckable(QAction *action);

void forceMenuButtonMode(QToolBar *toolBar, QAction *action);

template<class T>
concept IsAction = std::is_base_of_v<QAction, T>;

template<IsAction Action>
inline auto createProxyAction(Action *prototype, QObject *parent)
{
    return static_cast<Action *>(bindAction(new Action{parent}, prototype));
}

template<IsAction Action>
inline auto createProxyAction(Action *prototype)
{
    return createProxyAction(prototype, prototype);
}

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_ACTIONUTILS_H
