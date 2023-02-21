#include "functionmappingview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/fileformat.h>
#include <lmrs/core/memory.h>

#include <lmrs/esu/functionmappingmodel.h>
#include <lmrs/gui/fontawesome.h>

#include <QActionGroup>
#include <QBoxLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolBar>
#include <QToolButton>

namespace lmrs::studio {

namespace {

class AbstractMappingEditor : public QFrame
{
    Q_OBJECT

protected:
    template<class ItemView, bool = std::is_base_of_v<QAbstractItemView, ItemView>>
    AbstractMappingEditor(std::in_place_type_t<ItemView>, QWidget *parent = nullptr);

public:
    virtual void reset(QModelIndex index) = 0;
    virtual void store(QAbstractItemModel *model, QModelIndex index) = 0;

protected:
    QAbstractItemView *const m_itemView;
    core::ConstPointer<QStandardItemModel> m_itemModel{m_itemView};
};

class MappingConditionEditor : public AbstractMappingEditor
{
    Q_OBJECT

public:
    MappingConditionEditor(QWidget *parent = nullptr);

    void reset(QModelIndex index) override;
    void store(QAbstractItemModel *model, QModelIndex index) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onItemClicked(QModelIndex index);

    static QString stateName(esu::Condition condition);

    enum {
        ConditionStateRole = Qt::UserRole + 1,
        CheckStateRole,
    };

    const QIcon m_checkedIcon = icon(gui::fontawesome::farCircleDot);
    const QIcon m_uncheckedIcon = icon(gui::fontawesome::farCircle);
};

class MappingOperationEditor : public AbstractMappingEditor
{
public:
    MappingOperationEditor(QWidget *parent = nullptr);

    void reset(QModelIndex index) override;
    void store(QAbstractItemModel *model, QModelIndex index) override;

private:
    template<core::EnumType T>
    void reset(QModelIndex index);

    template<core::EnumType T>
    void store(QAbstractItemModel *model, QModelIndex index);
};

class MappingItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
};

template<class ItemView, bool>
AbstractMappingEditor::AbstractMappingEditor(std::in_place_type_t<ItemView>, QWidget *parent)
    : QFrame{parent, Qt::Popup}
    , m_itemView{new ItemView{this}}
{
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(m_itemView);

    const auto proxyModel = new QSortFilterProxyModel{m_itemView};
    proxyModel->setSourceModel(m_itemModel);
    proxyModel->setFilterKeyColumn(-1);

    m_itemView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_itemView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_itemView->setModel(proxyModel);

    const auto filter = new QLineEdit{this};
    filter->setPlaceholderText(tr("Filter..."));
    filter->setClearButtonEnabled(true);

    const auto layout = new QVBoxLayout{this};
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_itemView);
    layout->addWidget(filter);

    connect(filter, &QLineEdit::textChanged, proxyModel, [proxyModel](QString text) {
        proxyModel->setFilterFixedString(std::move(text));
    });
}

MappingConditionEditor::MappingConditionEditor(QWidget *parent)
    : AbstractMappingEditor(std::in_place_type<QTableView>, parent)
{
    const auto tableView = core::checked_cast<QTableView *>(m_itemView);

    tableView->installEventFilter(this);
    tableView->setShowGrid(false);

    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->hide();

    tableView->verticalHeader()->setDefaultSectionSize(12);
    tableView->verticalHeader()->hide();

    connect(tableView, &QTableView::clicked, this, &MappingConditionEditor::onItemClicked);
}

void MappingConditionEditor::reset(QModelIndex index)
{
    const auto mapping = qvariant_cast<esu::Mapping>(index.data(esu::FunctionMappingModel::MappingRole));
    const auto states = std::array{esu::Condition::Enabled, esu::Condition::Disabled, esu::Condition::Ignore};

    m_itemModel->clear();

    for (const auto entry: QMetaTypeId<esu::Condition::Variable>{}) {
        if (entry.value() == esu::Condition::Unused)
            continue;

        const auto state = mapping.state(entry.value());
        auto row = QList{new QStandardItem(esu::displayName(entry.value()))};
        row.last()->setData(QVariant::fromValue(entry.value()));
        row.last()->setEditable(false);

        for (auto i = 0U; i < states.size(); ++i) {
            const auto checkState = (state == states[i] ? Qt::Checked : Qt::Unchecked);

            row.append(new QStandardItem{stateName({entry.value(), states[i]})});
            row.last()->setIcon(checkState == Qt::Checked ? m_checkedIcon : m_uncheckedIcon);
            row.last()->setData(QVariant::fromValue(states[i]), ConditionStateRole);
            row.last()->setData(QVariant::fromValue(checkState), CheckStateRole);
            row.last()->setEditable(false);
        }

        m_itemModel->appendRow(std::move(row));
    }

    // only works with the model being filled
    const auto tableView = core::checked_cast<QTableView *>(m_itemView);
    tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}

void MappingConditionEditor::store(QAbstractItemModel *model, QModelIndex index)
{
    auto conditions = QList<esu::Condition>{};

    for (auto row = 0; row < m_itemModel->rowCount(); ++row) {
        const auto variable = qvariant_cast<esu::Condition::Variable>(m_itemModel->item(row, 0)->data());

        for (auto column = 1; column < m_itemModel->columnCount(); ++column) {
            const auto item = m_itemModel->item(row, column);
            const auto checkState = qvariant_cast<Qt::CheckState>(item->data(CheckStateRole));
            const auto state = qvariant_cast<esu::Condition::State>(item->data(ConditionStateRole));

            if (checkState == Qt::Checked)
                qInfo() << variable << state;

            if (checkState == Qt::Checked && state != esu::Condition::Ignore)
                conditions.append({variable, state});
        }
    }

    model->setData(index, QVariant::fromValue(std::move(conditions)), esu::FunctionMappingModel::ConditionsRole);
}

bool MappingConditionEditor::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_itemView && event->type() == QEvent::KeyPress) {
        const auto keyEvent = static_cast<const QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Space && !keyEvent->modifiers()) {
            onItemClicked(m_itemView->currentIndex());
            return true;
        }
    }

    return false;
}

void MappingConditionEditor::onItemClicked(QModelIndex index)
{
    if (index.column() < 1)
        return;

    for (auto column = 1; column < m_itemModel->columnCount(); ++column) {
        const auto checked = column == index.column();
        const auto item = m_itemModel->item(index.row(), column);
        item->setIcon(checked ? m_checkedIcon : m_uncheckedIcon);
        item->setData(checked ? Qt::Checked : Qt::Unchecked, CheckStateRole);
    }
}

// FIXME: move to model
QString MappingConditionEditor::stateName(esu::functionmapping::Condition condition)
{
    if (condition.variable == esu::Condition::Driving) {
        if (condition.state == esu::Condition::Enabled)
            return tr("Driving");
        else if (condition.state == esu::Condition::Disabled)
            return tr("Standing");
    } else if (condition.variable == esu::Condition::Direction) {
        if (condition.state == esu::Condition::Forward)
            return tr("Forward");
        else if (condition.state == esu::Condition::Reverse)
            return tr("Reverse");
    }

    switch (condition.state) {
    case esu::Condition::Enabled:
        return esu::FunctionMappingModel::tr("Enabled");

    case esu::Condition::Disabled:
        return esu::FunctionMappingModel::tr("Disabled");

    case esu::Condition::Ignore:
        return esu::FunctionMappingModel::tr("Ignore");
    }

    Q_UNREACHABLE();
    return {};
}

MappingOperationEditor::MappingOperationEditor(QWidget *parent)
    : AbstractMappingEditor(std::in_place_type<QListView>, parent)
{}

template<core::EnumType T>
void MappingOperationEditor::reset(QModelIndex index)
{
    const auto flags = qvariant_cast<QFlags<T>>(index.data(esu::FunctionMappingModel::ColumnDataRole));

    m_itemModel->clear();

    for (const auto entry: QMetaTypeId<T>{}) {
        auto item = std::make_unique<QStandardItem>(QString::fromLatin1(entry.key()));//FIXME: esu::displayName(entry.value()));

        item->setData(QVariant::fromValue(entry.value()));
        item->setEditable(false);
        item->setCheckable(true);
        item->setCheckState(flags.testFlag(entry.value()) ? Qt::Checked : Qt::Unchecked);

        m_itemModel->appendRow(item.release());
    }
}

void MappingOperationEditor::reset(QModelIndex index)
{
    switch (static_cast<esu::FunctionMappingModel::DataColumn>(index.column())) {
    case esu::FunctionMappingModel::ConditionColumn:
        break;

    case esu::FunctionMappingModel::OutputsColumn:
        reset<esu::Output>(std::move(index));
        return;

    case esu::FunctionMappingModel::EffectsColumn:
        reset<esu::Effect>(std::move(index));
        return;

    case esu::FunctionMappingModel::SoundsColumn:
        reset<esu::Sound>(std::move(index));
        return;
    }

    Q_UNREACHABLE();
}

template<core::EnumType T>
void MappingOperationEditor::store(QAbstractItemModel *model, QModelIndex index)
{
    auto mapping = qvariant_cast<esu::Mapping>(index.data(esu::FunctionMappingModel::MappingRole));
    auto flags = QFlags<T>{};

    for (auto row = 0; row < m_itemModel->rowCount(); ++row) {
        const auto item = m_itemModel->item(row);

        if (item->checkState() == Qt::Checked)
            flags |= qvariant_cast<T>(item->data());
    }

    mapping.set(flags);
    model->setData(index, QVariant::fromValue(std::move(mapping)), esu::FunctionMappingModel::MappingRole);
}

void MappingOperationEditor::store(QAbstractItemModel *model, QModelIndex index)
{
    switch (static_cast<esu::FunctionMappingModel::DataColumn>(index.column())) {
    case esu::FunctionMappingModel::ConditionColumn:
        break;

    case esu::FunctionMappingModel::OutputsColumn:
        store<esu::Output>(model, std::move(index));
        return;

    case esu::FunctionMappingModel::EffectsColumn:
        store<esu::Effect>(model, std::move(index));
        return;

    case esu::FunctionMappingModel::SoundsColumn:
        store<esu::Sound>(model, std::move(index));
        return;
    }

    Q_UNREACHABLE();
}

QWidget *MappingItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    switch (static_cast<esu::FunctionMappingModel::DataColumn>(index.column())) {
    case esu::FunctionMappingModel::ConditionColumn:
        return new MappingConditionEditor{parent->topLevelWidget()};

    case esu::FunctionMappingModel::OutputsColumn:
    case esu::FunctionMappingModel::EffectsColumn:
    case esu::FunctionMappingModel::SoundsColumn:
        return new MappingOperationEditor{parent->topLevelWidget()};
    }

    Q_UNREACHABLE();
    return nullptr;
}

void MappingItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    auto origin = option.widget->mapToGlobal(option.rect.topRight());
    origin.setY(qMax(20, origin.y() - 100));
    editor->move(std::move(origin));
}

void MappingItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if (const auto mappingEditor = core::checked_cast<AbstractMappingEditor *>(editor))
        mappingEditor->reset(index);
}

void MappingItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    if (const auto mappingEditor = core::checked_cast<AbstractMappingEditor *>(editor))
        mappingEditor->store(model, index);
}

} // namespace

class FunctionMappingView::Private : public core::PrivateObject<FunctionMappingView>
{
public:
    explicit Private(QAbstractItemModel *devices, FunctionMappingView *parent)
        : PrivateObject{parent}
        , devices{devices}
    {}

    esu::FunctionMappingModel *model() const { return core::checked_cast<esu::FunctionMappingModel *>(tableView->model()); }

    void importFromDevice();
    void importFromFile();

    void exportToDevice();
    void exportToFile();

    auto makeResetAction(esu::FunctionMappingModel::Preset preset)
    {
        return [this, preset] { model()->reset(preset); };
    };

    const QPointer<QAbstractItemModel> devices;
    core::ConstPointer<QTableView> tableView{q()};
};

FunctionMappingView::FunctionMappingView(QAbstractItemModel *devices, QWidget *parent)
    : QWidget{parent}
    , d{new Private{devices, this}}
{
    d->tableView->setModel(new esu::FunctionMappingModel{this});
    d->tableView->setItemDelegate(new MappingItemDelegate{this});
    d->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);

    const auto toolbar = new QToolBar{this};
    const auto fileNewAction = toolbar->addAction(icon(gui::fontawesome::fasFile), tr("Reset"),
                                                  d, d->makeResetAction(esu::FunctionMappingModel::Preset::Empty));
    const auto fileNewButton = core::checked_cast<QToolButton *>(toolbar->widgetForAction(fileNewAction));
    fileNewButton->setPopupMode(QToolButton::MenuButtonPopup);
    fileNewAction->setMenu(new QMenu{this});

    for (const auto preset: QMetaTypeId<esu::FunctionMappingModel::Preset>{}) {
        if (preset.value() != esu::FunctionMappingModel::Preset::Empty)
            fileNewAction->menu()->addAction(displayName(preset.value()), d, d->makeResetAction(preset.value()));
    }

    toolbar->addSeparator();
    toolbar->addAction(icon(gui::fontawesome::fasFolderOpen), tr("Open from Disk"), d, &Private::importFromFile);
    toolbar->addAction(icon(gui::fontawesome::fasFloppyDisk), tr("Save to Disk"), d, &Private::exportToFile);
    toolbar->addSeparator();
    const auto readFromVehicleAction = toolbar->addAction(icon(gui::fontawesome::fasFileImport),
                                                          tr("Read from Vehicle"), d, &Private::importFromDevice);
    const auto writeToVehicleAction = toolbar->addAction(icon(gui::fontawesome::fasFileExport),
                                                         tr("Write to Vehicle"), d, &Private::exportToDevice);
    toolbar->addSeparator();

    const auto variableSelection = new QActionGroup{this};
    variableSelection->addAction(tr("All variables"))->setCheckable(true);
    variableSelection->addAction(tr("Changed variables"))->setCheckable(true);
    variableSelection->setExclusive(true);
    variableSelection->actions().constFirst()->setChecked(true);
    toolbar->addActions(variableSelection->actions());

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(toolbar);
    layout->addWidget(d->tableView, 1);

    const auto updateVehicleActions = [this, readFromVehicleAction, writeToVehicleAction] {
        qInfo() << d->devices->rowCount();
        readFromVehicleAction->setEnabled(d->devices->rowCount() > 0);
        writeToVehicleAction->setEnabled(d->devices->rowCount() > 0);
    };

    connect(d->devices, &QAbstractItemModel::rowsInserted, this, updateVehicleActions);
    connect(d->devices, &QAbstractItemModel::rowsRemoved, this, updateVehicleActions);

    updateVehicleActions();
}

void FunctionMappingView::Private::importFromDevice()
{
    // FIXME: Do proper dialog for device, address, and options
    // FIXME: Replace QVariant::value<T>() with qvariant_cast<> all over the place
    const auto device = qvariant_cast<core::Device *>(devices->index(0, 0).data(Qt::UserRole));

    model()->read(device->variableControl(), 1770); // FIXME: use variable address
}

void FunctionMappingView::Private::importFromFile()
{
    // FIXME: Use proper documents folder from QDesktopServices, from QSettings
    auto filters = core::FileFormat::openFileDialogFilter(esu::FunctionMappingModel::readableFileFormats());
    const auto fileName = QFileDialog::getOpenFileName(q(), tr("Open Function Mapping"), {}, std::move(filters));

    if (!fileName.isEmpty() && !model()->read(std::move(fileName))) {
        QMessageBox::critical(q(), tr("Could not read function mapping"),
                              tr("<p>Could not read a function mappping from <b>%1</b>.</p><p>%2</p>").
                              arg(QFileInfo{std::move(fileName)}.fileName(), model()->errorString()));
    }
}

void FunctionMappingView::Private::exportToDevice()
{
    model()->write(nullptr, 1770); // FIXME: use variable address
}

void FunctionMappingView::Private::exportToFile()
{
    // FIXME: Use proper documents folder from QDesktopServices, from QSettings
    auto filters = core::FileFormat::saveFileDialogFilter(esu::FunctionMappingModel::writableFileFormats());
    const auto fileName = QFileDialog::getSaveFileName(q(), tr("Save Function Mapping"), {}, std::move(filters));

    if (!fileName.isEmpty() && !model()->write(std::move(fileName))) {
        QMessageBox::critical(q(), tr("Could not write function mapping"),
                              tr("<p>Could not write this function mapping to <b>%1</b>.</p><p>%2</p>").
                              arg(QFileInfo{std::move(fileName)}.fileName(), model()->errorString()));
    }
}

} // namespace lmrs::studio

#include "functionmappingview.moc"
