#include "packageexplorer.h"

#include "audiopreview.h"
#include "documentpreview.h"
#include "imagepreview.h"
#include "packagelistmodel.h"

#include <QBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QSplitter>
#include <QStackedWidget>

#include <lmrs/core/typetraits.h>

#include <QDebug>

using namespace lmrs; // FIXME

class PackageExplorer::Private : public core::PrivateObject<PackageExplorer>
{
public:
    using PrivateObject::PrivateObject;

    PackageListModel *const model = new PackageListModel{q()};
    QListView *const fileListView = new QListView{q()};
    QStackedWidget *const detailStack = new QStackedWidget{q()};
    AudioPreview *const audioPreview = new AudioPreview{detailStack};
    DocumentPreview *const documentPreview = new DocumentPreview{detailStack};
    ImagePreview *const imagePreview = new ImagePreview{detailStack};
    QLabel *const fallbackView = new QLabel{"Unsupported file type.", detailStack};

    AudioPreview staticAudioPreview;
};

PackageExplorer::PackageExplorer(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    const auto splitter = new QSplitter{this};

    d->fileListView->setSelectionMode(QListView::SingleSelection);
    d->fileListView->setMinimumWidth(200);
    d->fileListView->setModel(d->model);
    splitter->addWidget(d->fileListView);

    connect(d->fileListView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](auto index) {
        const auto data = index.data(PackageListModel::DataRole).toByteArray();

        if (d->audioPreview->load(data)) {
            d->detailStack->setCurrentWidget(d->audioPreview);
        } else if (d->documentPreview->load(data)) {
            d->detailStack->setCurrentWidget(d->documentPreview);
        } else if (d->imagePreview->load(data)) {
            d->detailStack->setCurrentWidget(d->imagePreview);
        } else {
            d->detailStack->setCurrentWidget(d->fallbackView);
        }
    });


    connect(d->fileListView, &QListView::activated, this, [this](auto index) {
        const auto data = index.data(PackageListModel::DataRole).toByteArray();

        if (d->staticAudioPreview.load(data))
            d->staticAudioPreview.play();
    });

    d->fallbackView->setAlignment(Qt::AlignCenter);

    d->detailStack->setMinimumSize(800, 600);
    d->detailStack->addWidget(d->fallbackView);
    d->detailStack->addWidget(d->audioPreview);
    d->detailStack->addWidget(d->documentPreview);
    d->detailStack->addWidget(d->imagePreview);

    splitter->addWidget(d->detailStack);
    splitter->setStretchFactor(1, 1);

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(splitter);
}

PackageExplorer::~PackageExplorer()
{
    delete d;
}

void PackageExplorer::setPackage(std::shared_ptr<esu::Package> package)
{
    d->fileListView->clearSelection();
    d->model->setPackage(std::move(package));
    d->fileListView->setCurrentIndex(d->model->index(0));
}

std::shared_ptr<esu::Package> PackageExplorer::package() const
{
    return d->model->package();
}
