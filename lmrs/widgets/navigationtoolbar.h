#ifndef LMRS_WIDGETS_NAVIGATIONTOOLBAR_H
#define LMRS_WIDGETS_NAVIGATIONTOOLBAR_H

#include <QToolBar>

class QStackedWidget;

namespace lmrs::core::l10n {
class String;
}

namespace lmrs::widgets {

class NavigationToolBar : public QToolBar
{
    Q_OBJECT

public:
    explicit NavigationToolBar(QWidget *parent = {});

    QAction *addView(QIcon icon, core::l10n::String text, QWidget *view);
    void addSeparator();

    void setCurrentView(QWidget *view);

    void attachMainWidget(QStackedWidget *stack);

    QList<QAction *> menuActions() const;

signals:
    void currentViewChanged(QWidget *view);

protected:
    bool event(QEvent *event) override;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_NAVIGATIONTOOLBAR
