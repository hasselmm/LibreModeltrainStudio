#include "navigationtoolbar.h"

#include "actionutils.h"

#include <lmrs/core/memory.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/gui/localization.h>

#include <QActionGroup>
#include <QEvent>
#include <QLayout>
#include <QStackedWidget>

namespace lmrs::widgets {

namespace {

QKeySequence createShortcut(int index)
{
    const auto key = core::value<int>(Qt::Key_0) + index % 10;

    if (index < 10)
        return QKeySequence{key | core::value<int>(Qt::Modifier::CTRL | Qt::Modifier::SHIFT)};

    return {};
}

auto makeButtonText(QString &text)
{
    return text.replace('/'_L1, "-\n"_L1);
};

auto makeMenuText(QString &text)
{
    return text.replace('/'_L1, QString{}).replace('\n'_L1, QString{});
}

} // namespace

class NavigationToolBar::Private : public core::PrivateObject<NavigationToolBar>
{
public:
    using PrivateObject::PrivateObject;

    void resizeButtonsToFullWidth();

    core::ConstPointer<QActionGroup> actionGroup{this};
    core::ConstPointer<QActionGroup> menuActionGroup{this};

protected:
    bool eventFilter(QObject *target, QEvent *event) override;
};

void NavigationToolBar::Private::resizeButtonsToFullWidth()
{
    const auto toolBarLayout = q()->layout();

    auto widestButtonWidth = 0;

    // find width of the most widest button
    for (int i = 0; i < toolBarLayout->count(); ++i) {
        const auto widget = toolBarLayout->itemAt(i)->widget();
        widestButtonWidth = qMax(widestButtonWidth, widget->minimumSizeHint().width());
    }

    // resize all buttons to match the widest button
    for (int i = 0; i < toolBarLayout->count(); ++i) {
        const auto item = toolBarLayout->itemAt(i);
        item->widget()->setFixedWidth(widestButtonWidth);
    }
}

bool NavigationToolBar::Private::eventFilter(QObject *target, QEvent *event)
{
    // synchronize enabled state of the views with its actions
    if (event->type() == QEvent::EnabledChange) {
        for (const auto action: actionGroup->actions()) {
            if (const auto view = action->data().value<QWidget *>(); view == target)
                action->setEnabled(view->isEnabled());
        }
    }

    return QObject::eventFilter(target, event);
}

NavigationToolBar::NavigationToolBar(QWidget *parent)
    : QToolBar{parent}
    , d{new Private{this}}
{
    setStyleSheet(R"(
        QToolBar {
            background: transparent;
            border: none;
        }

        QToolBar::separator {
            margin: 0.25em;
            height: 1px;
        }

        QToolButton {
            border: none;
            border-left: 0.25em solid transparent;
            width: 4em;
        }

        QToolButton:hover {
            background: palette(midlight);
            border-left-color: palette(mid);
        }

        QToolButton:checked {
            border-left-color: palette(highlight);
            background: palette(base);
            color: palette(highlight);
        }
    )"_L1);

    setAllowedAreas(Qt::LeftToolBarArea);
    setContextMenuPolicy(Qt::ContextMenuPolicy::ActionsContextMenu);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setFloatable(false);
    setMovable(false);

    d->actionGroup->setExclusive(true);
    d->menuActionGroup->setExclusive(true);
}

NavigationToolBar::~NavigationToolBar()
{
    delete d;
}

QAction *NavigationToolBar::addView(QIcon icon, l10n::String text, QWidget *view)
{
    view->installEventFilter(d);

    const auto buttonAction = new gui::l10n::Action{d->actionGroup};
    buttonAction->setShortcut(createShortcut(static_cast<int>(d->actionGroup->actions().count())));
    buttonAction->setData(QVariant::fromValue(view));
    buttonAction->setEnabled(view->isEnabled());
    buttonAction->setIcon(std::move(icon));
    buttonAction->setCheckable(true);
    addAction(buttonAction);

    connect(buttonAction, &QAction::triggered, this, [this, buttonAction, view] {
        if (buttonAction->isChecked())
            emit currentViewChanged(view);
    });

    const auto menuAction = createProxyAction(buttonAction, BindActionOption::IgnoreText, d->menuActionGroup);
    d->menuActionGroup->addAction(menuAction);

    buttonAction->setText({text, &makeButtonText});
    menuAction->setText({text, &makeMenuText});

    return buttonAction;
}

void NavigationToolBar::addSeparator()
{
    QToolBar::addSeparator();

    const auto separator = new QAction{d->menuActionGroup};
    separator->setSeparator(true);
}

void NavigationToolBar::setCurrentView(QWidget *view)
{
    for (const auto action: d->actionGroup->actions()) {
        if (action->data().value<QWidget *>() == view) {
            action->setChecked(true);
            break;
        }
    }
}

void NavigationToolBar::attachMainWidget(QStackedWidget *stack)
{
    auto onCurrentViewChanged = [this, stack] {
        setCurrentView(stack->currentWidget());
    };

    connect(this, &NavigationToolBar::currentViewChanged,
            stack, &QStackedWidget::setCurrentWidget);
    connect(stack, &QStackedWidget::currentChanged,
            this, std::move(onCurrentViewChanged));
}

QList<QAction *> NavigationToolBar::menuActions() const
{
    return d->menuActionGroup->actions();
}

bool NavigationToolBar::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest)
        d->resizeButtonsToFullWidth();

    return QToolBar::event(event);
}

} // namespace lmrs::widgets
