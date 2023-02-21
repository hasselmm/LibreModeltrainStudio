#ifndef LMRS_WIDGETS_NAVIGATIONTOOLBAR_H
#define LMRS_WIDGETS_NAVIGATIONTOOLBAR_H

#include <QToolBar>

class QStackedWidget;

namespace lmrs::widgets {

class NavigationToolBar : public QToolBar
{
    Q_OBJECT

public:
    explicit NavigationToolBar(QWidget *parent = {});
    ~NavigationToolBar() override;

    QAction *addView(QIcon icon, QString text, QWidget *view);
    void setCurrentView(QWidget *view);

    void attachMainWidget(QStackedWidget *stack);

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
