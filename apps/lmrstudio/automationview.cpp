#include "automationview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/automationmodel.h>
#include <lmrs/core/fileformat.h>
#include <lmrs/core/parameters.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/gui/fontawesome.h>

#include <lmrs/widgets/actionutils.h>
#include <lmrs/widgets/recentfilemenu.h>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QGridLayout>
#include <QIdentityProxyModel>
#include <QListView>
#include <QMessageBox>
#include <QMimeData>
#include <QQuickItem>
#include <QQuickWidget>
#include <QToolBar>

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
        qmlRegisterType<automation::Action>("Lmrs.Core.Automation", 1, 0, "Action");
        qmlRegisterType<automation::Event>("Lmrs.Core.Automation", 1, 0, "Event");
        qmlRegisterType<automation::Item>("Lmrs.Core.Automation", 1, 0, "Item");

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

private:
    using QIdentityProxyModel::setSourceModel;
    using QIdentityProxyModel::sourceModel;
};

} // namespace

class AutomationView::Private : public core::PrivateObject<AutomationView>
{
public:
    using PrivateObject::PrivateObject;

    void createActionItem(const automation::Action *prototype);
    void createEventItem(const automation::Event *prototype);

    void onClipboardChanged(QClipboard::Mode mode);
    void onCurrentIndexChanged();

    void onFileNew();
    void onFileOpen();
    void onFileSave();
    void onFileSaveAs();

    void onEditCut();
    void onEditCopy();
    void onEditPaste();
    void onEditDelete();

    void setModel(core::automation::AutomationModel *newModel);
    void markDirty() { qInfo() << __func__; setDirty(true); }
    void setDirty(bool isDirty);

    QDialog::DialogCode open(QString newFileName);
    QDialog::DialogCode maybeSaveChanges();
    QDialog::DialogCode saveChanges();

    void resetModel();

    bool hasPendingChanges = false;
    QString fileName;

    core::ConstPointer<AutomationTypeModel> types{this};
    core::ConstPointer<AutomationCanvas> canvas{q()};

    QHash<ActionCategory, QActionGroup *> actionGroups;
    core::ConstPointer<widgets::RecentFileMenu> recentFilesMenu{"AutomationView/RecentFiles"_L1, q()};

    core::ConstPointer<QAction> fileNewAction = createAction(icon(gui::fontawesome::fasFile),
                                                             tr("&New"), tr("Create new automation plan"),
                                                             QKeySequence::New, this, &Private::onFileNew);
    core::ConstPointer<QAction> fileOpenAction = createAction(icon(gui::fontawesome::fasFolderOpen),
                                                              tr("&Open..."), tr("Open automation plan from disk"),
                                                              QKeySequence::Open, this, &Private::onFileOpen);
    core::ConstPointer<QAction> fileOpenRecentAction = q()->addAction(tr("Recent&ly used files"));

    core::ConstPointer<QAction> fileSaveAction = createAction(icon(gui::fontawesome::fasFloppyDisk),
                                                              tr("&Save"), tr("&Save current automation plan to disk"),
                                                              QKeySequence::Save, this, &Private::onFileSave);
    core::ConstPointer<QAction> fileSaveAsAction = createAction(icon(gui::fontawesome::fasFloppyDisk),
                                                                tr("Save &as..."), tr("&Save current automation plan to disk, under new name"),
                                                                QKeySequence::SaveAs, this, &Private::onFileSaveAs);

    core::ConstPointer<QAction> editCutAction = createAction(icon(gui::fontawesome::fasScissors),
                                                             tr("C&ut"), tr("Copy currently selected item to clipboard, and then delete from view"),
                                                             QKeySequence::Cut, this, &Private::onEditCut);
    core::ConstPointer<QAction> editCopyAction = createAction(icon(gui::fontawesome::fasCopy),
                                                              tr("&Copy"), tr("Copy currently selected item to clipboard"),
                                                              QKeySequence::Copy, this, &Private::onEditCopy);
    core::ConstPointer<QAction> editPasteAction = createAction(icon(gui::fontawesome::fasPaste),
                                                               tr("&Paste"), tr("Insert new item from clipboard"),
                                                               QKeySequence::Paste, this, &Private::onEditPaste);
    core::ConstPointer<QAction> editDeleteAction = createAction(icon(gui::fontawesome::fasTrashCan),
                                                                tr("&Delete"), tr("Delete currently selected item"),
                                                                Qt::ControlModifier | Qt::Key_Delete,
                                                                this, &Private::onEditDelete);
};

AutomationView::AutomationView(QWidget *parent)
    : MainWindowView{parent}
    , d{new Private{this}}
{
    d->setModel(new AutomationModel{this});

    connect(QApplication::clipboard(), &QClipboard::changed, d, &Private::onClipboardChanged);
    connect(d->canvas, &AutomationCanvas::currentIndexChanged, d, &Private::onCurrentIndexChanged);

    d->onCurrentIndexChanged();
    d->onClipboardChanged(QClipboard::Selection);

    const auto toolBar = new QToolBar{this};
    const auto fileOpenToolBarAction = widgets::createProxyAction(d->fileOpenAction);

    toolBar->addAction(d->fileNewAction);
    toolBar->addAction(fileOpenToolBarAction);
    toolBar->addAction(d->fileSaveAction);
    toolBar->addSeparator();
    toolBar->addAction(d->editCutAction);
    toolBar->addAction(d->editCopyAction);
    toolBar->addAction(d->editPasteAction);
    toolBar->addAction(d->editDeleteAction);

    d->recentFilesMenu->bindMenuAction(d->fileOpenRecentAction);
    d->recentFilesMenu->bindToolBarAction(fileOpenToolBarAction, toolBar);

    connect(d->recentFilesMenu, &widgets::RecentFileMenu::fileSelected, this, &AutomationView::open);

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

    auto foo = new StyledAutomationTypeModel{typeSelector};
    foo->setSourceModel(d->types);
    typeSelector->setModel(foo);

    connect(typeSelector, &QListView::activated, this, [this](QModelIndex index) {
        if (const auto prototype = d->types->eventItem(index))
            d->createEventItem(prototype);
        else if (const auto prototype = d->types->actionItem(index))
            d->createActionItem(prototype);
    });

    // -----------------------------------------------------------------------------------------------------------------

    d->actionGroups[ActionCategory::FileNew] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileNew]->addAction(d->fileNewAction);

    d->actionGroups[ActionCategory::FileOpen] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileOpen]->addAction(d->fileOpenAction);
    d->actionGroups[ActionCategory::FileOpen]->addAction(d->fileOpenRecentAction);

    d->actionGroups[ActionCategory::FileSave] = new QActionGroup{this};
    d->actionGroups[ActionCategory::FileSave]->addAction(d->fileSaveAction);

    d->actionGroups[ActionCategory::EditCreate] = new QActionGroup{this};
    d->actionGroups[ActionCategory::EditCreate]->addAction(createEventAction);

    d->actionGroups[ActionCategory::EditClipboard] = new QActionGroup{this};
    d->actionGroups[ActionCategory::EditClipboard]->addAction(d->editCutAction);
    d->actionGroups[ActionCategory::EditClipboard]->addAction(d->editCopyAction);
    d->actionGroups[ActionCategory::EditClipboard]->addAction(d->editPasteAction);
    d->actionGroups[ActionCategory::EditClipboard]->addAction(d->editDeleteAction);

    // -----------------------------------------------------------------------------------------------------------------

    const auto layout = new QGridLayout{this};

    layout->addWidget(toolBar, 0, 0, 1, 2);
    layout->addWidget(d->canvas, 1, 0);
    layout->addWidget(typeSelector, 1, 1);

    d->setDirty(false);
}

QActionGroup *AutomationView::actionGroup(ActionCategory category) const
{
    return d->actionGroups[category];
}

bool AutomationView::open(QString newFileName)
{
    return d->open(std::move(newFileName)) == QDialog::Accepted;
}

void AutomationView::Private::createEventItem(const automation::Event *prototype)
{
    if (const auto model = canvas->model()) {
        const auto index = model->appendEvent(types->fromPrototype(prototype, model));
        canvas->setCurrentIndex(index);
    }
}

void AutomationView::Private::createActionItem(const automation::Action *prototype)
{
    if (const auto model = canvas->model()) {
        if (const auto eventIndex = model->index(canvas->currentIndex(), 0); eventIndex.isValid()) {
            const auto actionIndex = model->appendAction(eventIndex, types->fromPrototype(prototype, model));
            //canvas->setCurrentIndex(index);
            qInfo() << Q_FUNC_INFO << actionIndex;
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

    editCutAction->setEnabled(hasSelectedItem);
    editCopyAction->setEnabled(hasSelectedItem);
    editDeleteAction->setEnabled(hasSelectedItem);
}

void AutomationView::Private::onFileNew()
{
    if (maybeSaveChanges() == QDialog::Rejected)
        return;

    resetModel();
}

void AutomationView::Private::onFileOpen()
{
    if (maybeSaveChanges() == QDialog::Rejected)
        return;

    auto filters = core::FileFormat::openFileDialogFilter(AutomationModelReader::fileFormats());
    auto newFileName = QFileDialog::getOpenFileName(q(), tr("Open Automation Plan"), {}, std::move(filters));

    if (!newFileName.isEmpty())
        q()->open(std::move(newFileName));
}

void AutomationView::Private::onFileSave()
{
    saveChanges();
}

void AutomationView::Private::onFileSaveAs()
{
    auto restoreFileName = qScopeGuard([this, oldFileName = fileName] {
        fileName = std::move(oldFileName);
    });

    fileName.clear();

    if (saveChanges() == QDialog::Accepted)
        restoreFileName.dismiss();
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
            if (const auto event = types->fromJson<automation::Event>(std::move(json), canvas->model())) {
                const auto index = canvas->model()->insertEvent(event, canvas->currentIndex() + 1);
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
        setDirty(false);

        if (newModel) {
            connect(newModel, &AutomationModel::actionChanged, this, &Private::markDirty);
            connect(newModel, &AutomationModel::eventChanged, this, &Private::markDirty);
            connect(newModel, &AutomationModel::rowsInserted, this, &Private::markDirty);
            connect(newModel, &AutomationModel::rowsMoved, this, &Private::markDirty);
            connect(newModel, &AutomationModel::rowsRemoved, this, &Private::markDirty);
        }
    }
}

void AutomationView::Private::setDirty(bool isDirty)
{
    hasPendingChanges = isDirty;
    fileSaveAction->setEnabled(hasPendingChanges);
}

QDialog::DialogCode AutomationView::Private::open(QString newFileName)
{
    if (maybeSaveChanges() == QDialog::Rejected)
        return QDialog::Rejected;

    const auto reader = AutomationModelReader::fromFile(newFileName);

    if (auto newModel = reader->read(types); !newModel) {
        const auto fileInfo = QFileInfo{reader->fileName()};

        QMessageBox::warning(q(), tr("Reading failed"),
                             tr("Reading the automation plan <b>\"%1\"</b> from \"%2\" has failed.<br><br>%3").
                             arg(fileInfo.fileName(), fileInfo.filePath(), reader->errorString()));

        return QDialog::Rejected;
    } else {
        fileName = std::move(newFileName);
        recentFilesMenu->addFileName(fileName);
        setModel(newModel.release());
        return QDialog::Accepted;
    }
}

QDialog::DialogCode AutomationView::Private::maybeSaveChanges()
{
    if (!hasPendingChanges)
        return QDialog::Accepted;

    const auto result = QMessageBox::question(q(), tr("Save changes"),
                                              tr("There are unsaved changes to your automation plan. "
                                                 "Do you want to save them first?"),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (result == QMessageBox::Cancel)
        return QDialog::Rejected;

    if (result == QMessageBox::Discard) {
        resetModel();
        return QDialog::Accepted;
    }

    return saveChanges();
}

QDialog::DialogCode AutomationView::Private::saveChanges()
{
    const auto fileNameGuard = core::propertyGuard(q(), &AutomationView::fileName, &AutomationView::fileNameChanged);

    if (fileName.isEmpty()) {
        auto filters = core::FileFormat::saveFileDialogFilter(AutomationModelWriter::fileFormats());
        fileName = QFileDialog::getSaveFileName(q(), tr("Save Automation Plan"), {}, std::move(filters));

        if (fileName.isEmpty())
            return QDialog::Rejected;
    }

    const auto writer = AutomationModelWriter::fromFile(fileName);

    if (!writer->write(canvas->model())) {
        const auto fileInfo = QFileInfo{writer->fileName()};

        QMessageBox::warning(q(), tr("Saving failed"),
                             tr("Saving your automation plan <b>\"%1\"</b> to \"%2\" has failed.<br><br>%3").
                             arg(fileInfo.fileName(), fileInfo.filePath(), writer->errorString()));

        return QDialog::Rejected;
    }

    setDirty(false);
    recentFilesMenu->addFileName(fileName);

    return QDialog::Accepted;
}

void AutomationView::Private::resetModel()
{
    fileName.clear();
    canvas->model()->clear();
}

} // namespace lmrs::studio

#include "automationview.moc"
