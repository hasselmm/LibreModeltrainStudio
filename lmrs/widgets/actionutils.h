#ifndef LMRS_WIDGETS_ACTIONUTILS_H
#define LMRS_WIDGETS_ACTIONUTILS_H

#include <QAbstractItemModel>
#include <QPointer>
#include <QSizePolicy>
#include <QWidgetAction>

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

class ComboBoxAction : public QWidgetAction
{
    Q_OBJECT

public:
    using QWidgetAction::QWidgetAction;

    QWidget *createWidget(QWidget *parent) override;

    void setModel(QAbstractItemModel *newModel);
    QAbstractItemModel *model() const;

    void setSizePolicy(QSizePolicy::Policy newPolicy);
    QSizePolicy::Policy sizePolicy() const;

    void setMinimumContentsLength(int newLength);
    int minimumContentsLength() const;

    void setCurrentIndex(QModelIndex newIndex);
    void setCurrentIndex(int newIndex);
    int currentIndex() const;

signals:
    void modelChanged(QAbstractItemModel *model, QPrivateSignal);
    void currentIndexChanged(int currentIndex, QPrivateSignal);

private:
    void updateVisiblity();

    QPointer<QAbstractItemModel> m_model;
    QSizePolicy::Policy m_sizePolicy = QSizePolicy::Fixed;
    int m_minimumContentsLength = 3;
    int m_currentIndex = -1;
};

class SpacerAction : public QWidgetAction
{
    Q_OBJECT

public:
    using QWidgetAction::QWidgetAction;

    QWidget *createWidget(QWidget *parent) override;
};

class ZoomLevelModel : public QAbstractListModel
{
public:
    explicit ZoomLevelModel(QObject *parent);

    void setRange(int newMinimumZoom, int newMaximumZoom);

    int minimumZoom() const;
    int minimumLevel() const;

    int maximumZoom() const;
    int maximumLevel() const;

    int toLevel(int value) const;
    int toValue(int level) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = {}) const override;

private:
    int m_minimumLevel = 0;
    int m_maximumLevel = 0;
    int m_step = 25;
};

class ZoomActionGroup : public QObject
{
    Q_OBJECT

public:
    explicit ZoomActionGroup(QObject *parent);

    void setCurrentZoom(int newCurrentZoom);
    int currentZoom() const;

    ZoomLevelModel *model() const;
    QList<QAction *> actions() const;

signals:
    void currentZoomChanged(int currentZoom);

private:
    void onZoomIn();
    void onZoomOut();

    void setCurrentLevel(int newLevel);

    int m_currentLevel = 0;

    QAction *const m_zoomInAction;
    QAction *const m_zoomOutAction;
    ComboBoxAction *const m_pickerAction;
};

} // namespace lmrs::widgets

Q_DECLARE_OPERATORS_FOR_FLAGS(lmrs::widgets::BindActionOptions)

#endif // LMRS_WIDGETS_ACTIONUTILS_H
