#include "automationview.h"
#include "deviceconnectionview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/automationmodel.h>
#include <lmrs/core/fileformat.h>
#include <lmrs/core/parameters.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/gui/fontawesome.h>
#include <lmrs/gui/localization.h>

#include <lmrs/widgets/actionutils.h>
#include <lmrs/widgets/documentmanager.h>
#include <lmrs/widgets/recentfilemenu.h>

#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QFileDialog>
#include <QIdentityProxyModel>
#include <QListView>
#include <QMessageBox>
#include <QMimeData>
#include <QQuickItem>
#include <QQuickWidget>

namespace lmrs::studio {

namespace {

namespace automation = core::automation;

using core::automation::AutomationModel;
using core::automation::AutomationModelReader;
using core::automation::AutomationModelWriter;
using core::automation::AutomationTypeModel;

const auto s_property_currentIndex = "currentIndex"_BV;
const auto s_property_model = "model"_BV;

class AutomationCanvas : public QQuickWidget
{
    Q_OBJECT

public:
    explicit AutomationCanvas(QWidget *parent = nullptr)
        : QQuickWidget{parent}
    {
        qmlRegisterUncreatableType<automation::Action>("Lmrs.Core.Automation", 1, 0, "Action",
                                                       "Cannot create instances of an abstract type"_L1);
        qmlRegisterUncreatableType<automation::Event>("Lmrs.Core.Automation", 1, 0, "Event",
                                                      "Cannot create instances of an abstract type"_L1);
        qmlRegisterUncreatableType<automation::Item>("Lmrs.Core.Automation", 1, 0, "Item",
                                                     "Cannot create instances of an abstract type"_L1);

        qmlRegisterType<automation::Parameter>("Lmrs.Core.Automation", 1, 0, "parameter");

        qmlRegisterSingletonType("qrc:/taschenorakel.de/lmrs/studio/qml/Style.qml"_url,
                                 "Lmrs.Studio", 1, 0, "Style");

        setResizeMode(QQuickWidget::ResizeMode::SizeRootObjectToView);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        setSource("qrc:/taschenorakel.de/lmrs/studio/qml/AutomationView.qml"_url);

        if (const auto root = rootObject()) {
            connect(root, SIGNAL(currentIndexChanged()),
                    this, SIGNAL(currentIndexChanged()));
        }
    }

    void setModel(AutomationModel *model)
    {
        if (const auto root = rootObject())
            root->setProperty(s_property_model.constData(), QVariant::fromValue(model));
    }

    AutomationModel *model() const
    {
        if (const auto root = rootObject())
            return qvariant_cast<AutomationModel *>(root->property(s_property_model.constData()));

        return nullptr;
    }

    const AutomationModel *constModel() const
    {
        return const_cast<const AutomationModel *>(model());
    }

    void setCurrentIndex(int index)
    {
        if (const auto root = rootObject())
            root->setProperty(s_property_currentIndex.constData(), index);
    }

    int currentIndex() const
    {
        if (const auto root = rootObject())
            return root->property(s_property_currentIndex.constData()).toInt();

        return -1;
    }

signals:
    void currentIndexChanged();
};

class StyledAutomationTypeModel : public QIdentityProxyModel
{
public:
    using QIdentityProxyModel::QIdentityProxyModel;

    void setSourceModel(AutomationTypeModel *sourceModel)
    {
        QIdentityProxyModel::setSourceModel(sourceModel);
    }

    AutomationTypeModel *sourceModel() const
    {
        return core::checked_cast<AutomationTypeModel* >(QIdentityProxyModel::sourceModel());
    }

    void setActionsEnabled(bool newEnabled)
    {
        if (const auto oldEnabled = std::exchange(m_actionsEnabled, newEnabled); oldEnabled != newEnabled)
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role == Qt::DecorationRole) {
            if (sourceModel()->eventItem(mapToSource(index)))
                return icon(gui::fontawesome::fasBell);
            if (sourceModel()->actionItem(mapToSource(index)))
                return icon(gui::fontawesome::fasWandMagicSparkles);
        }

        return QIdentityProxyModel::data(index, role);
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        auto sourceIndex = mapToSource(index);
        auto flags = sourceModel()->flags(sourceIndex);

        if (sourceModel()->actionItem(std::move(sourceIndex)))
            flags.setFlag(Qt::ItemIsEnabled, m_actionsEnabled);

        return flags;
    }

private:
    using QIdentityProxyModel::setSourceModel;
    using QIdentityProxyModel::sourceModel;

    bool m_actionsEnabled = false;
};

} // namespace

class AutomationView::Private : public core::PrivateObject<AutomationView, widgets::DocumentManager, QString>
{
public:
    using PrivateObject::PrivateObject;

    // widgets::DocumentManager interface ------------------------------------------------------------------------------

    QString documentType() const override { return tr("automation plan"); }
    QList<core::FileFormat> readableFileFormats() const override { return AutomationModelReader::fileFormats(); }
    QList<core::FileFormat> writableFileFormats() const override { return AutomationModelWriter::fileFormats(); }

    FileHandlerPointer readFile(QString fileName) override;
    FileHandlerPointer writeFile(QString fileName) override;
    void resetModel(QVariant) override;

    // model handling --------------------------------------------------------------------------------------------------

    void createActionItem(const automation::Action *prototype);
    void createEventItem(const automation::Event *prototype);

    void onClipboardChanged(QClipboard::Mode mode);
    void onCurrentIndexChanged();

    void onEditCut();
    void onEditCopy();
    void onEditPaste();
    void onEditDelete();

    void setModel(core::automation::AutomationModel *newModel);

    // fields ----------------------------------------------------------------------------------------------------------

    core::ConstPointer<AutomationTypeModel> types{this};
    core::ConstPointer<StyledAutomationTypeModel> styledTypes{this};

    core::ConstPointer<AutomationCanvas> canvas{q()};

    QHash<ActionCategory, QActionGroup *> actionGroups;

    core::ConstPointer<l10n::Action> fileNewAction{icon(gui::fontawesome::fasFile),
                LMRS_TR("&New"), LMRS_TR("Create new automation plan"),
                QKeySequence::New, this, &DocumentManager::reset};
    core::ConstPointer<l10n::Action> fileOpenAction{icon(gui::fontawesome::fasFolderOpen),
                LMRS_TR("&Open..."), LMRS_TR("Open automation plan from disk"),
                QKeySequence::Open, this, &DocumentManager::open};

    core::ConstPointer<l10n::Action> fileOpenRecentAction{LMRS_TR("Recent&ly used files"), this};

    core::ConstPointer<l10n::Action> fileSaveAction{icon(gui::fontawesome::fasFloppyDisk),
                LMRS_TR("&Save"), LMRS_TR("Save current automation plan to disk"),
                QKeySequence::Save, this, &DocumentManager::save};
    core::ConstPointer<l10n::Action> fileSaveAsAction{
                LMRS_TR("Save &as..."), LMRS_TR("Save current automation plan to disk, under new name"),
                QKeySequence::SaveAs, this, &DocumentManager::saveAs};

    core::ConstPointer<l10n::Action> editCutAction{icon(gui::fontawesome::fasScissors),
                LMRS_TR("C&ut"), LMRS_TR("Copy currently selected item to clipboard, and then delete from view"),
                QKeySequence::Cut, this, &Private::onEditCut};
    core::ConstPointer<l10n::Action> editCopyAction{icon(gui::fontawesome::fasCopy),
                LMRS_TR("&Copy"), LMRS_TR("Copy currently selected item to clipboard"),
                QKeySequence::Copy, this, &Private::onEditCopy};
    core::ConstPointer<l10n::Action> editPasteAction{icon(gui::fontawesome::fasPaste),
                LMRS_TR("&Paste"), LMRS_TR("Insert new item from clipboard"),
                QKeySequence::Paste, this, &Private::onEditPaste};
    core::ConstPointer<l10n::Action> editDeleteAction{icon(gui::fontawesome::fasTrashCan),
                LMRS_TR("&Delete"), LMRS_TR("Delete currently selected item"),
                Qt::ControlModifier | Qt::Key_Delete,
                this, &Private::onEditDelete};
};

AutomationView::AutomationView(QWidget *parent)
    : MainWindowView{parent}
    , d{new Private{"AutomationView"_L1, this}}
{
    d->setModel(new AutomationModel{this});

    connect(QApplication::clipboard(), &QClipboard::changed, d, &Private::onClipboardChanged);
    connect(d->canvas, &AutomationCanvas::currentIndexChanged, d, &Private::onCurrentIndexChanged);

    d->onCurrentIndexChanged();
    d->onClipboardChanged(QClipboard::Selection);
    d->recentFilesMenu()->bindMenuAction(d->fileOpenRecentAction);

    const auto typesGroup = new QActionGroup{this};

    for (const auto &index: d->types) {
        if (const auto prototype = d->types->eventItem(index)) {
            connect(typesGroup->addAction(prototype->name()), &QAction::triggered, this, [this, prototype] {
                d->createEventItem(prototype);
            });
        }
    }

    const auto createEventAction = new QAction{tr("&Create"), this};

    createEventAction->setMenu(new QMenu{this});
    createEventAction->menu()->addActions(typesGroup->actions());

    auto typeSelector = new QListView{this};

    typeSelector->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    typeSelector->setResizeMode(QListView::ResizeMode::Adjust);
    typeSelector->setSelectionMode(QListView::NoSelection);
    typeSelector->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    typeSelector->setUniformItemSizes(true);

    d->styledTypes->setSourceModel(d->types);
    typeSelector->setModel(d->styledTypes);

    connect(typeSelector, &QListView::activated, this, [this](QModelIndex index) {
        if (const auto prototype = d->types->eventItem(index))
            d->createEventItem(prototype);
        else if (const auto prototype = d->types->actionItem(index))
            d->createActionItem(prototype);
    });

    // -----------------------------------------------------------------------------------------------------------------

    // FIXME: move fileOpenAction, fileSaveAction, and fileSaveAsAction into document manager
    connect(d, &Private::modifiedChanged, d->fileSaveAction, &QAction::setEnabled);
    d->fileSaveAction->setEnabled(d->isModified());

    connectDocumentManager(d);

    // -----------------------------------------------------------------------------------------------------------------

    d->actionGroups[ActionCategory::FileNew] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileNew]->addAction(d->fileNewAction);

    d->actionGroups[ActionCategory::FileOpen] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileOpen]->addAction(d->fileOpenAction);
    d->actionGroups[ActionCategory::FileOpen]->addAction(d->fileOpenRecentAction);

    d->actionGroups[ActionCategory::FileSave] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileSave]->addAction(d->fileSaveAction);
    d->actionGroups[ActionCategory::FileSave]->addAction(d->fileSaveAsAction);

    d->actionGroups[ActionCategory::EditCreate] = new QActionGroup{this};
    d->actionGroups[ActionCategory::EditCreate]->addAction(createEventAction);

    d->actionGroups[ActionCategory::EditClipboard] = new QActionGroup{this};
    d->actionGroups[ActionCategory::EditClipboard]->addAction(d->editCutAction);
    d->actionGroups[ActionCategory::EditClipboard]->addAction(d->editCopyAction);
    d->actionGroups[ActionCategory::EditClipboard]->addAction(d->editPasteAction);
    d->actionGroups[ActionCategory::EditClipboard]->addAction(d->editDeleteAction);

    // -----------------------------------------------------------------------------------------------------------------

    const auto layout = new QHBoxLayout{this};

    layout->addWidget(d->canvas, 1);
    layout->addWidget(typeSelector);

    d->resetModified();
}

QList<QActionGroup *> AutomationView::actionGroups(ActionCategory category) const
{
    return {d->actionGroups[category]};
}

AutomationView::FileState AutomationView::fileState() const
{
    return d->isModified() ? FileState::Modified : FileState::Saved;
}

QString AutomationView::fileName() const
{
    return d->fileName();
}

DeviceFilter AutomationView::deviceFilter() const
{
    return acceptAny<
            DeviceFilter::Require<core::AccessoryControl>,
            DeviceFilter::Require<core::DetectorControl>,
            DeviceFilter::Require<core::VehicleControl>>();
}

bool AutomationView::open(QString newFileName)
{
    return d->readFile(std::move(newFileName))->succeeded();
}

void AutomationView::updateControls(core::Device *device)
{
    d->canvas->model()->setDevice(device);
}

void AutomationView::Private::createEventItem(const automation::Event *prototype)
{
    if (const auto model = canvas->model()) {
        if (auto event = types->fromPrototype(prototype, model)) {
            const auto index = model->appendEvent(event.release());
            canvas->setCurrentIndex(index);
        }
    }
}

void AutomationView::Private::createActionItem(const automation::Action *prototype)
{
    if (const auto model = canvas->model()) {
        if (const auto eventIndex = model->index(canvas->currentIndex(), 0); eventIndex.isValid()) {
            if (auto action = types->fromPrototype(prototype, model)) {
                const auto actionIndex = model->appendAction(eventIndex, action.release());
                //canvas->setCurrentIndex(index);
                qInfo() << Q_FUNC_INFO << actionIndex;
            }
        }
    }
}

void AutomationView::Private::onClipboardChanged(QClipboard::Mode mode)
{
    if (mode == QClipboard::Clipboard) {
        if (const auto mimeData = QApplication::clipboard()->mimeData(mode)) {
            const auto hasAutomationEvent = mimeData->hasFormat(core::FileFormat::lmrsAutomationEvent().mimeType);
            editPasteAction->setEnabled(hasAutomationEvent);
        }
    }
}

void AutomationView::Private::onCurrentIndexChanged()
{
    // FIXME: also handle action and binding items

    const auto hasSelectedItem = (canvas->currentIndex() >= 0);
    const auto hasSelectedEvent = (canvas->model() && canvas->model()->eventItem(canvas->currentIndex()));

    editCutAction->setEnabled(hasSelectedItem);
    editCopyAction->setEnabled(hasSelectedItem);
    editDeleteAction->setEnabled(hasSelectedItem);
    styledTypes->setActionsEnabled(hasSelectedEvent);
}

void AutomationView::Private::onEditCut()
{
    onEditCopy();
    onEditDelete();
}

void AutomationView::Private::onEditCopy()
{
    if (const auto model = canvas->constModel()) {
        if (const auto event = model->eventItem(canvas->currentIndex())) {
            auto json = event->toJson();
            auto mimeData = std::make_unique<QMimeData>();
            mimeData->setText(QString::fromUtf8(event->toJson()));
            mimeData->setData(core::FileFormat::lmrsAutomationEvent().mimeType, std::move(json));
            QApplication::clipboard()->setMimeData(mimeData.release());
        }
    }
}

void AutomationView::Private::onEditPaste()
{
    if (const auto mimeData = QApplication::clipboard()->mimeData()) {
        if (const auto json = mimeData->data(core::FileFormat::lmrsAutomationEvent().mimeType); !json.isEmpty()) {
            if (auto event = types->fromJson<automation::Event>(std::move(json), canvas->model())) {
                const auto index = canvas->model()->insertEvent(event.release(), canvas->currentIndex() + 1);
                canvas->setCurrentIndex(index);
            }
        }
    }
}

void AutomationView::Private::onEditDelete()
{
    if (const auto model = canvas->model())
        model->removeRow(canvas->currentIndex());
}

void AutomationView::Private::setModel(AutomationModel *newModel)
{
    if (const auto oldModel = canvas->model(); oldModel != newModel) {
        if (oldModel)
            oldModel->disconnect(this);

        canvas->setModel(newModel);
        resetModified();

        if (newModel) {
            connect(newModel, &AutomationModel::actionChanged, this, &DocumentManager::markModified);
            connect(newModel, &AutomationModel::eventChanged, this, &DocumentManager::markModified);
            connect(newModel, &AutomationModel::rowsInserted, this, &DocumentManager::markModified);
            connect(newModel, &AutomationModel::rowsMoved, this, &DocumentManager::markModified);
            connect(newModel, &AutomationModel::rowsRemoved, this, &DocumentManager::markModified);
        }
    }
}

AutomationView::Private::FileHandlerPointer AutomationView::Private::readFile(QString fileName)
{
    auto reader = AutomationModelReader::fromFileName(std::move(fileName));

    if (auto newModel = reader->read(types))
        setModel(newModel.release());

    return reader;
}

AutomationView::Private::FileHandlerPointer AutomationView::Private::writeFile(QString fileName)
{
    auto writer = AutomationModelWriter::fromFileName(std::move(fileName));
    const auto succeeded = writer->write(canvas->model());
    Q_ASSERT(succeeded == writer->succeeded());
    return writer;
}

void AutomationView::Private::resetModel(QVariant)
{
    canvas->model()->clear();
}

} // namespace lmrs::studio

#include "automationview.moc"
