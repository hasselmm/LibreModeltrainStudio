#ifndef LMRS_WIDGETS_ACTIONUTILS_H
#define LMRS_WIDGETS_ACTIONUTILS_H

class QAbstractButton;
class QAction;
class QToolBar;

namespace lmrs::widgets {

QAbstractButton *bindAction(QAbstractButton *target, QAction *action);
QAction *bindAction(QAction *target, QAction *action);
QAction *createProxyAction(QAction *prototype);

void forceMenuButtonMode(QToolBar *toolBar, QAction *action);

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_ACTIONUTILS_H
