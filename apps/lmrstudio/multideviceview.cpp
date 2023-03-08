#include "multideviceview.h"

#include <lmrs/core/device.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QDebug>
#include <QPointer>
#include <QTabBar>
#include <QTabWidget>

class ModelIndexIterator // FIXME: Move to public header for sharing
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = ptrdiff_t;
    using value_type        = QModelIndex;
    using pointer           = value_type*;
    using reference         = value_type&;

    ModelIndexIterator(QModelIndex index)
        : m_index{std::move(index)}
    {}

    ModelIndexIterator &operator++() { m_index = m_index.siblingAtRow(m_index.row() + 1); return *this; }
    const QModelIndex *operator->() const { return &m_index; }
    const QModelIndex &operator*() const { return m_index; }

    [[nodiscard]] bool operator==(const ModelIndexIterator &rhs) const noexcept = default;

private:
    QModelIndex m_index;
};

struct ModelRange // FIXME: Move to public header for sharing
{
public:
    ModelRange(QModelIndex first, QModelIndex last)
        : m_begin{first}
        , m_end{last.siblingAtRow(last.row() + 1)}
    {
        Q_ASSERT(!m_end->isValid() || m_begin->model() == m_end->model());
        Q_ASSERT(!m_end->isValid() || m_begin->parent() == m_end->parent());
    }

    ModelRange(QAbstractItemModel *model, QModelIndex parent, int first, int last)
        : ModelRange{model->index(first, 0, parent), model->index(last, 0, parent)}
    {}

    auto begin() const { return m_begin; }
    auto end() const { return m_end; }

private:
    ModelIndexIterator m_begin;
    ModelIndexIterator m_end;
};

// FIXME: Move to public header for sharing
inline ModelIndexIterator begin(const QAbstractItemModel *model) { return {model->index(0, 0)}; }
inline ModelIndexIterator end(const QAbstractItemModel *model) { return {model->index(0, model->rowCount() - 1)}; }

namespace lmrs::studio {

class AbstractMultiDeviceView::Private : public core::PrivateObject<AbstractMultiDeviceView>
{
public:
    using PrivateObject::PrivateObject;

    void onModelReset();
    void onRowsInserted(const QModelIndex parent, int first, int last);
    void onRowsRemoved(const QModelIndex parent, int first, int last);
    void onDataChanged(const QModelIndex topLeft, const QModelIndex bottomRight, const QList<int> roles);

    void updateWidgetState();

    QPointer<QAbstractItemModel> model;
    core::ConstPointer<QTabWidget> tabs{q()};
};

void AbstractMultiDeviceView::Private::onModelReset()
{
    tabs->clear();

    for (const auto &index: model.get()) {
        const auto device = index.data(Qt::UserRole).value<core::Device *>();
        const auto deviceTab = q()->createWidget(device, tabs);
        tabs->addTab(deviceTab, index.data().toString());
    }

    updateWidgetState();
}

void AbstractMultiDeviceView::Private::onRowsInserted(const QModelIndex parent, int first, int last)
{
    if (parent.isValid())
        return;

    for (const auto &index: ModelRange{model, parent, first, last}) {
        const auto device = index.data(Qt::UserRole).value<core::Device *>();
        const auto deviceTab = q()->createWidget(device, tabs);
        tabs->insertTab(index.row(), deviceTab, index.data().toString());
    }

    updateWidgetState();
}

void AbstractMultiDeviceView::Private::onRowsRemoved(const QModelIndex parent, int first, int last)
{
    if (parent.isValid())
        return;

    for (int row = last; row >= first; --row)
        tabs->removeTab(row);

    updateWidgetState();
}

void AbstractMultiDeviceView::Private::onDataChanged(const QModelIndex topLeft, const QModelIndex bottomRight, const QList<int> /*roles*/)
{
    if (topLeft.parent().isValid())
        return;
    if (bottomRight.parent().isValid())
        return;

    for (const auto &index: ModelRange{topLeft, bottomRight})
        tabs->setTabText(index.row(), index.data().toString());
}

void AbstractMultiDeviceView::Private::updateWidgetState()
{
    static const auto s_baseStyle = R"(
        lmrs--studio--AbstractMultiDeviceView {
            background: transparent;
            border: none;
        }

        lmrs--studio--AbstractMultiDeviceView > QTabWidget::pane {
            background: transparent;
            border: none;
        }

        lmrs--studio--AbstractMultiDeviceView > QTabWidget::tab-bar {
            alignment: left;
        }

        lmrs--studio--AbstractMultiDeviceView > QTabWidget > *::tab {
            background: transparent;

            border: none;
            border-right: 0.35ex solid transparent;


            min-height: 5em;
            padding: 0.25em;
            padding-left: 0.25ex;
            padding-right: 0px;
        }

        lmrs--studio--AbstractMultiDeviceView > QTabWidget > *::tab:hover {
            background: palette(midlight);
            border-left-color: palette(mid);
        }

        lmrs--studio--AbstractMultiDeviceView > QTabWidget > *::tab:selected {
            background: palette(base);
            border-right-color: palette(highlight);
            color: palette(text);
        }
    )"_L1;

    static const auto s_borderStyle = s_baseStyle + R"(
        lmrs--studio--AbstractMultiDeviceView {
            border-left: 0.5ex solid palette(base);
        }

        lmrs--studio--AbstractMultiDeviceView > QTabWidget::pane {
            border-left: 0.25ex solid palette(highlight);
        }
    )"_L1;

    q()->setStyleSheet(tabs->count() >= 2 ? s_borderStyle : s_baseStyle);
    q()->setEnabled(tabs->count() > 0);
}

AbstractMultiDeviceView::AbstractMultiDeviceView(QWidget *parent)
    : QFrame{parent}
    , d{new Private{this}}
{
    d->tabs->setTabPosition(QTabWidget::West);
    d->tabs->setTabBarAutoHide(true);
    d->updateWidgetState();

    const auto layout = new QVBoxLayout{this};

    layout->addWidget(d->tabs);
    layout->setContentsMargins({});
}

AbstractMultiDeviceView::AbstractMultiDeviceView(QAbstractItemModel *model, QWidget *parent)
    : AbstractMultiDeviceView{parent}
{
    setModel(model);
}

void AbstractMultiDeviceView::setModel(QAbstractItemModel *model)
{
    if (const auto oldModel = std::exchange(d->model, model); oldModel != model) {
        if (oldModel)
            oldModel->disconnect(this);

        if (d->model) {
            connect(d->model, &QAbstractItemModel::modelReset, d, &Private::onModelReset);
            connect(d->model, &QAbstractItemModel::rowsInserted, d, &Private::onRowsInserted);
            connect(d->model, &QAbstractItemModel::rowsRemoved, d, &Private::onRowsRemoved);
            connect(d->model, &QAbstractItemModel::dataChanged, d, &Private::onDataChanged);
        }

        d->onModelReset();
    }
}

QAbstractItemModel *AbstractMultiDeviceView::model() const
{
    return d->model;
}

} // namespace lmrs::studio
