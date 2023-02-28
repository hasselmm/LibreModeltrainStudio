#include "actionutils.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/gui/localization.h>
#include <lmrs/gui/fontawesome.h>

#include <QBoxLayout>
#include <QComboBox>
#include <QMenu>
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

QWidget *ComboBoxAction::createWidget(QWidget *parent)
{
    if (const auto menu = dynamic_cast<QMenu *>(parent)) {
        Q_UNIMPLEMENTED(); // FIXME: Find solution to show devices in overflow menu
        return nullptr;
    }

    const auto comboBox = new QComboBox{parent};

    comboBox->setModel(m_model);
    comboBox->setSizePolicy(m_sizePolicy, QSizePolicy::Fixed);
    comboBox->setMinimumContentsLength(m_minimumContentsLength);
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    comboBox->setCurrentIndex(m_currentIndex);

    connect(this, &ComboBoxAction::modelChanged, comboBox, &QComboBox::setModel);
    connect(comboBox, &QComboBox::currentIndexChanged, this, qOverload<int>(&ComboBoxAction::setCurrentIndex));

    return comboBox;
}

void ComboBoxAction::setModel(QAbstractItemModel *newModel)
{
    if (const auto oldModel = std::exchange(m_model, newModel); oldModel != newModel) {
        if (oldModel)
            oldModel->disconnect(this);

        if (newModel) {
            connect(newModel, &QAbstractItemModel::rowsInserted, this, &ComboBoxAction::updateVisiblity);
            connect(newModel, &QAbstractItemModel::rowsRemoved, this, &ComboBoxAction::updateVisiblity);
            connect(newModel, &QAbstractItemModel::modelReset, this, &ComboBoxAction::updateVisiblity);
        }

        updateVisiblity();
        emit modelChanged(m_model, {});
    }
}

QAbstractItemModel *ComboBoxAction::model() const
{
    return m_model;
}

void ComboBoxAction::setSizePolicy(QSizePolicy::Policy newPolicy)
{
    if (const auto oldPolicy = std::exchange(m_sizePolicy, newPolicy); oldPolicy != newPolicy) {
        for (const auto widgetList = createdWidgets(); const auto widget: widgetList) {
            if (const auto comboBox = dynamic_cast<QComboBox *>(widget))
                comboBox->setSizePolicy(m_sizePolicy, QSizePolicy::Fixed);
        }
    }
}

QSizePolicy::Policy ComboBoxAction::sizePolicy() const
{
    return m_sizePolicy;
}

void ComboBoxAction::setMinimumContentsLength(int newLength)
{
    if (const auto oldLength = std::exchange(m_minimumContentsLength, newLength); oldLength != newLength) {
        for (const auto widgetList = createdWidgets(); const auto widget: widgetList) {
            if (const auto comboBox = dynamic_cast<QComboBox *>(widget))
                comboBox->setMinimumContentsLength(newLength);
        }
    }
}

int ComboBoxAction::minimumContentsLength() const
{
    return m_minimumContentsLength;
}

void ComboBoxAction::setCurrentIndex(QModelIndex newIndex)
{
    if (newIndex.isValid() && LMRS_FAILED(core::logger(this), newIndex.model() == model()))
        return;

    setCurrentIndex(newIndex.isValid() ? newIndex.row() : -1);
}

void ComboBoxAction::setCurrentIndex(int newIndex)
{
    if (const auto oldIndex = std::exchange(m_currentIndex, newIndex); oldIndex != newIndex) {
        for (const auto widgetList = createdWidgets(); const auto widget: widgetList) {
            if (const auto comboBox = dynamic_cast<QComboBox *>(widget))
                comboBox->setCurrentIndex(m_currentIndex);
        }

        emit currentIndexChanged(m_currentIndex, {});
    }
}

void ComboBoxAction::updateVisiblity()
{
    setVisible(m_model && m_model->rowCount() > 0);
}

QWidget *SpacerAction::createWidget(QWidget *parent)
{
    const auto spacer = new QWidget{parent};
    const auto spacerLayout = new QHBoxLayout{spacer};
    spacerLayout->addStretch();
    return spacer;
}

ZoomLevelModel::ZoomLevelModel(QObject *parent)
    : QAbstractListModel{parent}
{
    setRange(25, 400);
}

void ZoomLevelModel::setRange(int newMinimumZoom, int newMaximumZoom)
{
    const auto newMinimumLevel = toLevel(newMinimumZoom);
    const auto newMaximumLevel = toLevel(newMaximumZoom);

    if (newMinimumLevel != m_minimumLevel
            || newMaximumLevel != m_maximumLevel) {
        beginResetModel();
        m_minimumLevel = newMinimumLevel;
        m_maximumLevel = newMaximumLevel;
        endResetModel();

        /*
        auto model = dynamic_cast<QStandardItemModel *>(pickerAction()->model());

        if (model) {
            model->clear();
        } else {
            model = new QStandardItemModel{this};
        }

        for (auto level = m_minimumLevel; level <= m_maximumLevel; ++level) {
            const auto item = new QStandardItem{}
            model->appendRow()
                   }
    */
    }
}

int ZoomLevelModel::minimumZoom() const
{
    return toValue(m_minimumLevel);
}

int ZoomLevelModel::maximumZoom() const
{
    return toValue(m_maximumLevel);
}

int ZoomLevelModel::minimumLevel() const
{
    return m_minimumLevel;
}

int ZoomLevelModel::maximumLevel() const
{
    return m_maximumLevel;
}

int ZoomLevelModel::toLevel(int value) const
{
    return qRound(log(value / m_step) / log(2));
    //    if (const auto view = currentView())
    //        view->setTileSize(qMin(25 * (1 << qFloor(log(view->tileSize() / 25.0) / log(2) + 1)), 400));
}

int ZoomLevelModel::toValue(int level) const
{
    return (1 << level) * m_step;
    //    if (const auto view = currentView())
    //        view->setTileSize(qMax(25 * (1 << qFloor(log((view->tileSize() - 1) / 25.0) / log(2))), 25));
}

QVariant ZoomLevelModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        if (role == Qt::DisplayRole)
            return (QLocale{}.toString(toValue(index.row()))) + u'\u202f' + u'%';
        else if (role == Qt::UserRole)
            return toValue(index.row());
        else if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }

    return {};
}

int ZoomLevelModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return m_maximumLevel + 1;
}

ZoomActionGroup::ZoomActionGroup(QObject *parent)
    : QObject{parent}
    , m_zoomInAction{new l10n::Action{icon(gui::fontawesome::fasMagnifyingGlassPlus),
                     LMRS_TR("Zoom &in"), LMRS_TR("Zoom into the view"),
                     this, &ZoomActionGroup::onZoomIn}}
    , m_zoomOutAction{new l10n::Action{icon(gui::fontawesome::fasMagnifyingGlassMinus),
                      LMRS_TR("Zoom &out"), LMRS_TR("Zoom out of the view"),
                      this, &ZoomActionGroup::onZoomOut}}
    , m_pickerAction{new ComboBoxAction{this}}
{
    m_pickerAction->setModel(new ZoomLevelModel{m_pickerAction});
    m_pickerAction->setSizePolicy(QSizePolicy::Fixed);
    m_pickerAction->setMinimumContentsLength(3);

    connect(m_pickerAction, &ComboBoxAction::currentIndexChanged, this, &ZoomActionGroup::setCurrentLevel);

    setCurrentLevel(model()->toLevel(100));
}

void ZoomActionGroup::setCurrentZoom(int newCurrentZoom)
{
    if (const auto zoomModel = model())
        setCurrentLevel(zoomModel->toLevel(newCurrentZoom));
}

int ZoomActionGroup::currentZoom() const
{
    return model()->toValue(m_currentLevel);
}

ZoomLevelModel *ZoomActionGroup::model() const
{
    return core::checked_cast<ZoomLevelModel *>(m_pickerAction->model());
}

QList<QAction *> ZoomActionGroup::actions() const
{
    return {m_zoomOutAction, m_pickerAction, m_zoomInAction};
}

void ZoomActionGroup::onZoomIn()
{
    setCurrentLevel(m_currentLevel + 1);
}

void ZoomActionGroup::onZoomOut()
{
    setCurrentLevel(m_currentLevel - 1);
}

void ZoomActionGroup::setCurrentLevel(int newLevel)
{
    if (const auto zoomModel = model()) {
        newLevel = qBound(zoomModel->minimumLevel(), newLevel, zoomModel->maximumLevel());
        if (const auto oldLevel = std::exchange(m_currentLevel, newLevel); oldLevel != newLevel) {
            const auto currentValue = zoomModel->toValue(m_currentLevel);
            m_zoomInAction->setEnabled(currentValue < zoomModel->maximumZoom());
            m_zoomOutAction->setEnabled(currentValue > zoomModel->minimumZoom());
            m_pickerAction->setCurrentIndex(m_currentLevel);
            emit currentZoomChanged(currentValue);
        }
    }
}

} // namespace lmrs::widgets

#include "actionutils.moc"
