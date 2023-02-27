#ifndef LMRS_WIDGETS_ACTIONUTILS_H
#define LMRS_WIDGETS_ACTIONUTILS_H

#include <QFlags>

class QAbstractButton;
class QAction;
class QObject;
class QToolBar;

namespace lmrs::widgets {

enum class BindActionOption
{
    IgnoreIcon = (1 << 0),
    IgnoreText = (1 << 1),
    IgnoreToolTip = (1 << 2),
};

Q_DECLARE_FLAGS(BindActionOptions, BindActionOption);

QAbstractButton *bindAction(QAbstractButton *target, QAction *action, BindActionOptions options = {});
QAction *bindAction(QAction *target, QAction *action, BindActionOptions options = {});
QAction *makeCheckable(QAction *action);

void forceMenuButtonMode(QToolBar *toolBar, QAction *action);

template<class T>
concept IsAction = std::is_base_of_v<QAction, T>;

template<IsAction Action>
inline auto createProxyAction(Action *prototype, BindActionOptions options, QObject *parent)
{
    return static_cast<Action *>(bindAction(new Action{parent}, prototype, options));
}

template<IsAction Action>
inline auto createProxyAction(Action *prototype, QObject *parent)
{
    return createProxyAction(prototype, BindActionOptions{}, parent);
}

template<IsAction Action>
inline auto createProxyAction(Action *prototype, BindActionOptions options = {})
{
    return createProxyAction(prototype, options, prototype);
}

} // namespace lmrs::widgets

Q_DECLARE_OPERATORS_FOR_FLAGS(lmrs::widgets::BindActionOptions)

#endif // LMRS_WIDGETS_ACTIONUTILS_H
