#include "soundsloteditor.h"

#include "soundslottablemodel.h"

#include <lmrs/core/typetraits.h>

#include <QBoxLayout>
#include <QHeaderView>
#include <QTableView>


using namespace lmrs; // FIXME

class SoundSlotEditor::Private : public core::PrivateObject<SoundSlotEditor>
{
public:
    using PrivateObject::PrivateObject;

    SoundSlotTableModel *const model = new SoundSlotTableModel{q()};
    QTableView *const tableView = new QTableView{q()};
};

SoundSlotEditor::SoundSlotEditor(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    d->tableView->setModel(d->model);
    d->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    d->tableView->horizontalHeader()->setSectionResizeMode(SoundSlotTableModel::DescriptionColumn, QHeaderView::Stretch);
    d->tableView->verticalHeader()->setDefaultSectionSize(16);

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(d->tableView);
}

void SoundSlotEditor::setMetaData(std::shared_ptr<esu::MetaData> metaData)
{
    d->model->setMetaData(std::move(metaData));
}

std::shared_ptr<esu::MetaData> SoundSlotEditor::metaData() const
{
    return d->model->metaData();
}
