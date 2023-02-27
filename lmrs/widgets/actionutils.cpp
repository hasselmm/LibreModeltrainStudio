#include "actionutils.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/typetraits.h>

#include <lmrs/gui/localization.h>

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
    qInfo() << Q_FUNC_INFO << newIndex;
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

} // namespace lmrs::widgets

#include "actionutils.moc"
