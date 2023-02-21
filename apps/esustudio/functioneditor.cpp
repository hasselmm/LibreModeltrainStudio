#include "functioneditor.h"

#include "functiontablemodel.h"

#include <lmrs/core/typetraits.h>

#include <QBoxLayout>
#include <QHeaderView>
#include <QTableView>

using namespace lmrs; // FIXME

class FunctionEditor::Private : public core::PrivateObject<FunctionEditor>
{
public:
    using PrivateObject::PrivateObject;

    FunctionTableModel *const model = new FunctionTableModel{q()};
    QTableView *const tableView = new QTableView{q()};
};

FunctionEditor::FunctionEditor(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    d->tableView->setModel(d->model);
    d->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    d->tableView->horizontalHeader()->setSectionResizeMode(FunctionTableModel::DescriptionColumn, QHeaderView::Stretch);
    d->tableView->verticalHeader()->setDefaultSectionSize(16);

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(d->tableView);
}

void FunctionEditor::setMetaData(std::shared_ptr<esu::MetaData> metaData)
{
    d->model->setMetaData(std::move(metaData));
}

std::shared_ptr<esu::MetaData> FunctionEditor::metaData() const
{
    return d->model->metaData();
}
