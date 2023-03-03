#include "functionmappingview.h"

#include "deviceconnectionview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/fileformat.h>
#include <lmrs/core/memory.h>

#include <lmrs/esu/functionmappingmodel.h>

#include <lmrs/gui/fontawesome.h>
#include <lmrs/gui/localization.h>

#include <lmrs/widgets/documentmanager.h>
#include <lmrs/widgets/recentfilemenu.h>

#include <QActionGroup>
#include <QBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>

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

class FunctionMappingView::Private : public core::PrivateObject<FunctionMappingView, widgets::DocumentManager, QString>
{
public:
    using PrivateObject::PrivateObject;

    // widgets::DocumentManager interface ------------------------------------------------------------------------------

    QString documentType() const override { return tr("function mapping"); }
    QList<core::FileFormat> readableFileFormats() const override { return esu::FunctionMappingReader::fileFormats(); }
    QList<core::FileFormat> writableFileFormats() const override { return esu::FunctionMappingWriter::fileFormats(); }

    FileHandlerPointer readFile(QString fileName) override;
    FileHandlerPointer writeFile(QString fileName) override;
    void resetModel(QVariant model) override;

    // model handling --------------------------------------------------------------------------------------------------

    using Preset = esu::FunctionMappingModel::Preset;

    esu::FunctionMappingModel *currentModel() const
    {
        return core::checked_cast<esu::FunctionMappingModel *>(tableView->model());
    }

    void setupActions();

    void onReadFromVehicle();
    void onWriteToVehicle();

    std::function<void()> makeResetAction(Preset preset)
    {
        return [this, preset] { currentModel()->reset(preset); };
    };

    core::ConstPointer<l10n::Action> fileNewAction{icon(gui::fontawesome::fasFile),
                LMRS_TR("&New"), LMRS_TR("Create new function mapping"),
                this, makeResetAction(esu::FunctionMappingModel::Preset::Empty)};
    core::ConstPointer<l10n::Action> fileOpenAction{icon(gui::fontawesome::fasFolderOpen),
                LMRS_TR("&Open..."), LMRS_TR("Read a new function mapping from Disk"),
                QKeySequence::Open, this, &DocumentManager::open};

    core::ConstPointer<l10n::Action> fileOpenRecentAction{LMRS_TR("Recent&ly used files"), this};

    core::ConstPointer<l10n::Action> fileSaveAction{icon(gui::fontawesome::fasFloppyDisk),
                LMRS_TR("&Save"), LMRS_TR("Save current function mapping to Disk"),
                QKeySequence::Save, this, &DocumentManager::save};
    core::ConstPointer<l10n::Action> fileSaveAsAction{
                LMRS_TR("Save &as..."), LMRS_TR("Save current function mapping to disk, under new name"),
                QKeySequence::SaveAs, this, &DocumentManager::saveAs};

    core::ConstPointer<l10n::Action> readFromVehicleAction{icon(gui::fontawesome::fasFileImport),
                LMRS_TR("Read from Vehicle"), LMRS_TR("Read a new function from vehicle"),
                this, &Private::onReadFromVehicle};

    core::ConstPointer<l10n::Action> writeToVehicleAction{icon(gui::fontawesome::fasFileExport),
                LMRS_TR("Write to Vehicle"), LMRS_TR("Write current new function to vehicle"),
                this, &Private::onWriteToVehicle};

    l10n::ActionGroup *makeActionGroup(ActionCategory category);

    QHash<ActionCategory, QList<QActionGroup *>> actionGroups;

    core::ConstPointer<QTableView> tableView{q()};
    QPointer<core::VariableControl> variableControl;
};

void FunctionMappingView::Private::setupActions()
{
    recentFilesMenu()->bindMenuAction(fileOpenRecentAction);

    fileNewAction->setMenu(new QMenu{q()});

    for (const auto preset: QMetaTypeId<esu::FunctionMappingModel::Preset>{}) {
        if (preset.value() != esu::FunctionMappingModel::Preset::Empty) {
            fileNewAction->menu()->addAction(displayName(preset.value()),  // FIXME: use l10n::Action
                                             this, makeResetAction(preset.value()));
        }
    }

    const auto fileNewGroup = makeActionGroup(ActionCategory::FileNew);
    fileNewGroup->addAction(fileNewAction.get());

    const auto fileOpenGroup = makeActionGroup(ActionCategory::FileOpen);
    fileOpenGroup->addAction(fileOpenAction.get());
    fileOpenGroup->addAction(fileOpenRecentAction.get());

    const auto fileSaveGroup = makeActionGroup(ActionCategory::FileSave);
    fileSaveGroup->addAction(fileSaveAction.get());
    fileSaveGroup->addAction(fileSaveAsAction.get());

    const auto filePeripheralsGroup = makeActionGroup(ActionCategory::FilePeripherals);
    filePeripheralsGroup->addAction(readFromVehicleAction.get());
    filePeripheralsGroup->addAction(writeToVehicleAction.get());

    const auto fileExportModeGroup = makeActionGroup(ActionCategory::FilePeripherals);
    fileExportModeGroup->addAction(icon(gui::fontawesome::farRectangleXmark), LMRS_TR("Modified"),
                                   LMRS_TR("Only send modified variables to vehicle"))->setCheckable(true);
    fileExportModeGroup->addAction(icon(gui::fontawesome::farRectangleList), LMRS_TR("All"),
                                   LMRS_TR("Send all variables to vehicle"))->setCheckable(true);
    fileExportModeGroup->actions().constFirst()->setChecked(true);
    fileExportModeGroup->setProperty("toolbarOnly", true);
    fileExportModeGroup->setExclusive(true);

    readFromVehicleAction->setEnabled(false);
    writeToVehicleAction->setEnabled(false);

    // FIXME: move to DocumentManger or connectDocumentManager()
    connect(this, &Private::modifiedChanged, fileSaveAction, &QAction::setEnabled);
    fileSaveAction->setEnabled(isModified());
}

FunctionMappingView::FunctionMappingView(QWidget *parent)
    : MainWindowView{parent}
    , d{new Private{"FunctionMappingView"_L1, this}}
{
    d->setupActions();

    connectDocumentManager(d);

    d->tableView->setModel(new esu::FunctionMappingModel{this});
    d->tableView->setItemDelegate(new MappingItemDelegate{this});
    d->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(d->tableView, 1);
}

QList<QActionGroup *> FunctionMappingView::actionGroups(ActionCategory category) const
{
    return d->actionGroups.value(category);
}

FunctionMappingView::FileState FunctionMappingView::fileState() const
{
    return d->isModified() ? FileState::Modified : FileState::Saved;
}

QString FunctionMappingView::fileName() const
{
    return d->fileName();
}

DeviceFilter FunctionMappingView::deviceFilter() const
{
    return DeviceFilter::require<core::VariableControl>();
}

void FunctionMappingView::updateControls(core::Device *newDevice)
{
    setVariableControl(newDevice ? newDevice->variableControl() : nullptr);
}

void FunctionMappingView::setVariableControl(core::VariableControl *newControl)
{
    if (const auto oldControl = std::exchange(d->variableControl, newControl); oldControl != newControl) {
        d->readFromVehicleAction->setEnabled(newControl != nullptr);
        d->writeToVehicleAction->setEnabled(newControl != nullptr);
    }
}

core::VariableControl *FunctionMappingView::variableControl() const
{
    return d->variableControl;
}

void FunctionMappingView::Private::onReadFromVehicle()
{
    // FIXME: Do proper dialog for device, address, and options
    // FIXME: Replace QVariant::value<T>() with qvariant_cast<> all over the place

    if (variableControl)
        currentModel()->read(variableControl, 1770); // FIXME: use variable address
}

void FunctionMappingView::Private::onWriteToVehicle()
{
    currentModel()->write(nullptr, 1770); // FIXME: use variable address
}

l10n::ActionGroup *FunctionMappingView::Private::makeActionGroup(ActionCategory category)
{
    const auto actionGroup = new l10n::ActionGroup{this};
    actionGroups[category].append(actionGroup);
    return actionGroup;
}

FunctionMappingView::Private::FileHandlerPointer FunctionMappingView::Private::readFile(QString fileName)
{
    auto reader = esu::FunctionMappingReader::fromFile(std::move(fileName));

    if (auto newModel = reader->read()) {
        const auto oldModel = currentModel();
        tableView->setModel(newModel.release());
        delete oldModel;
    }

    return reader;
}

FunctionMappingView::Private::FileHandlerPointer FunctionMappingView::Private::writeFile(QString fileName)
{
    auto writer = esu::FunctionMappingWriter::fromFile(std::move(fileName));
    const auto succeeded = writer->write(currentModel());
    Q_ASSERT(succeeded == writer->succeeded());
    return writer;
}

void FunctionMappingView::Private::resetModel(QVariant model)
{
    if (const auto preset = core::get_if<Preset>(std::move(model)))
        if (const auto model = currentModel())
            model->reset(preset.value());
}

} // namespace lmrs::studio

#include "functionmappingview.moc"
