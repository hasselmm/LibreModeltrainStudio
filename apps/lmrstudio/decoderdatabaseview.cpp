#include "decoderdatabaseview.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/dccconstants.h>
#include <lmrs/core/decoderinfo.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QTreeView>

namespace lmrs::studio {

namespace {

using namespace core::dcc;

// =====================================================================================================================

enum class StaticVendorId {
    Any = 0,
    NRMA = -1,
    RailCommunity = -2,
};

// =====================================================================================================================

class VendorListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    VendorListModel(QObject *parent = {});

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &index = {}) const override;

private:
    static std::pair<int, QString> sortKey(int vendorId);
    static QString vendorName(int vendorId);

    QList<int> m_vendorIds;
};

class DecoderTypeModel : public QAbstractListModel
{
    Q_OBJECT

public:
    DecoderTypeModel(QObject *parent = {});

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &index = {}) const override;

    static QString decoderName(QString decoderId);
    static QString vendorFilter(int vendorId);

private:
    QStringList m_decoderIds;
};

class DecoderInfoModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    using QAbstractItemModel::QAbstractItemModel;

    void setDecoderId(QString decoderId);
    QString decoderId() const;

public: // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    enum class NodeType : uint { Root, Decoder, Variable };

    static quintptr internalId(NodeType nodeType, const QModelIndex &parent);
    static NodeType nodeType(const QModelIndex &index);
    static int parentRow(const QModelIndex &index);

    QList<DecoderInfo> m_decoders;
};

// =====================================================================================================================

VendorListModel::VendorListModel(QObject *parent)
    : QAbstractListModel{parent}
{
    auto uniqueVendorIds = QSet<int>{static_cast<int>(StaticVendorId::Any), };

    for (const auto knownDecoderIds = DecoderInfo::knownDecoderIds();
         const auto &decoderId: knownDecoderIds) {
        const auto vendorKey = decoderId.split(':'_L1).first();
        if (vendorKey == "NMRA"_L1)
            uniqueVendorIds.insert(static_cast<int>(StaticVendorId::NRMA));
        else if (vendorKey == "RCN"_L1)
            uniqueVendorIds.insert(static_cast<int>(StaticVendorId::RailCommunity));
        else if (const auto vendorId = vendorKey.toInt(); vendorId > 0)
            uniqueVendorIds.insert(vendorId);
        else
            qCWarning(core::logger(this), "Supported vendor id %ls", qUtf16Printable(vendorKey));
    }

    m_vendorIds = uniqueVendorIds.values();

    std::sort(m_vendorIds.begin(), m_vendorIds.end(), [](int lhs, int rhs) {
        return sortKey(lhs) < sortKey(rhs);
    });
}

QVariant VendorListModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        if (role == Qt::DisplayRole)
            return vendorName(m_vendorIds[index.row()]);
        else if (role == Qt::UserRole)
            return m_vendorIds[index.row()];
    }

    return {};
}

int VendorListModel::rowCount(const QModelIndex &index) const
{
    if (Q_UNLIKELY(index.isValid()))
        return 0;

    return static_cast<int>(m_vendorIds.count());
}

std::pair<int, QString> VendorListModel::sortKey(int vendorId) // FIXME: also apply these roles to decoder list model
{
    if (vendorId == core::value(StaticVendorId::Any))
        return {0, {}};
    if (vendorId < core::value(StaticVendorId::Any))
        return {1, vendorName(vendorId)};

    return {vendorId + 2, vendorName(vendorId)};
}

QString VendorListModel::vendorName(int vendorId)
{
    if (vendorId > core::value(StaticVendorId::Any))
        return DecoderInfo::vendorName(static_cast<VendorId::value_type>(vendorId));

    switch (static_cast<StaticVendorId>(vendorId)) {
    case StaticVendorId::NRMA:
        return tr("NMRA");
    case StaticVendorId::RailCommunity:
        return tr("RailCommunity");
    case StaticVendorId::Any:
        break;
    }

    return tr("Any Vendor");
}

// =====================================================================================================================

DecoderTypeModel::DecoderTypeModel(QObject *parent)
    : QAbstractListModel{parent}
    , m_decoderIds{DecoderInfo::knownDecoderIds(DecoderInfo::NoAliases)}
{
    std::sort(m_decoderIds.begin(), m_decoderIds.end(), [](const auto &lhs, const auto &rhs) {
        return decoderName(lhs) < decoderName(rhs);
    });
}

QVariant DecoderTypeModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        if (role == Qt::DisplayRole)
            return decoderName(m_decoderIds[index.row()]);
        else if (role == Qt::UserRole)
            return m_decoderIds[index.row()];
    }

    return {};
}

int DecoderTypeModel::rowCount(const QModelIndex &index) const
{
    return !index.isValid() ? static_cast<int>(m_decoderIds.count()) : 0;
}

QString DecoderTypeModel::decoderName(QString decoderId)
{
    return core::coalesce(DecoderInfo{decoderId}.name(), decoderId);
}

QString DecoderTypeModel::vendorFilter(int vendorId)
{
    if (vendorId > 0)
        return QString::number(vendorId) + ":*"_L1;

    switch (static_cast<StaticVendorId>(vendorId)) {
    case StaticVendorId::NRMA:
        return "NMRA:*"_L1;
    case StaticVendorId::RailCommunity:
        return "RCN:*"_L1;
    case StaticVendorId::Any:
        break;
    }

    return {};
}

// =====================================================================================================================

void DecoderInfoModel::setDecoderId(QString decoderId)
{
    auto decoders = QList<DecoderInfo>{};

    for (auto decoder = DecoderInfo{std::move(decoderId)}; decoder.isValid(); decoder = decoder.parent())
        decoders.emplaceBack(std::move(decoder));

    beginResetModel();
    std::swap(m_decoders, decoders);
    endResetModel();
}

QModelIndex DecoderInfoModel::index(int row, int column, const QModelIndex &parent) const
{
    switch (nodeType(parent)) {
    case NodeType::Root:
        return createIndex(row, column, internalId(NodeType::Decoder, parent));
    case NodeType::Decoder:
        return createIndex(row, column, internalId(NodeType::Variable, parent));
    case NodeType::Variable:
        break;
    }

    return {};
}

QModelIndex DecoderInfoModel::parent(const QModelIndex &child) const
{
    switch (nodeType(child)) {
    case NodeType::Root:
    case NodeType::Decoder:
        break;
    case NodeType::Variable:
        return createIndex(parentRow(child), 0, internalId(NodeType::Decoder, {}));
    }

    return {};
}

int DecoderInfoModel::rowCount(const QModelIndex &parent) const
{
    switch (nodeType(parent)) {
    case NodeType::Root:
        return static_cast<int>(m_decoders.count());
    case NodeType::Decoder:
        return static_cast<int>(m_decoders[parent.row()].variableIds(DecoderInfo::NoParent).count());
    case NodeType::Variable:
        break;
    }

    return 0;
}

int DecoderInfoModel::columnCount(const QModelIndex &parent) const
{
    switch (nodeType(parent)) {
    case NodeType::Root:
        return 2;
    case NodeType::Decoder:
        return 2;
    case NodeType::Variable:
        break;
    }

    return 0;
}

QVariant DecoderInfoModel::data(const QModelIndex &index, int role) const
{
    static const auto decoderData = [](DecoderInfo decoder, int role) -> QVariant {
        if (role == Qt::DisplayRole)
            return core::coalesce(decoder.name(), decoder.id());

        return {};
    };

    static const auto variableNameX = [](ExtendedVariableIndex extendedIndex)
    {
        const auto baseIndex = variableIndex(extendedIndex);

        if (range(VariableSpace::Extended).contains(baseIndex)) {
            return u"CV%1.%2 (CV31=%3, CV32=%4)"_qs.
                    arg(QString::number(baseIndex),
                        QString::number(extendedPage(extendedIndex)),
                        QString::number(cv31(extendedIndex)),
                        QString::number(cv32(extendedIndex)));
        } else if (range(VariableSpace::Susi).contains(baseIndex)) {
            return u"CV%1.%2 (SUSI %3)"_qs.
                    arg(QString::number(baseIndex),
                        QString::number(susiPage(extendedIndex)),
                        QString::number(core::value(susiNode(extendedIndex))));
        } else {
            return u"CV%1"_qs.arg(QString::number(extendedIndex));
        }
    };

    static const auto variableData = [](DecoderInfo decoder, int row, int column, int role) -> QVariant {
        const auto variableId = decoder.variableIds(DecoderInfo::NoParent).at(row);

        switch (column) {
        case 0:
            if (role == Qt::DisplayRole)
                return variableNameX(variableId);
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);

            break;

        case 1:
            if (role == Qt::DisplayRole)
                return decoder.variable(variableId, DecoderInfo::NoParent).name();

            break;
        }

        // FIXME: implement colors for decoder info view
        /*
        const auto variableInfo = decoderInfo.variable(vid);
        const auto variableItemId = new QStandardItem{variableName(static_cast<VehicleVariable>(vid))};
        const auto variableItemName = new QStandardItem{variableInfo.name()};

        if (unsupported.contains(vid)) {
            variableItemId->setForeground(Qt::red);
            variableItemName->setForeground(Qt::red);
        } else if (overrides.contains(vid)) {
            variableItemId->setForeground(Qt::magenta);
            variableItemName->setForeground(Qt::magenta);
        }
        overrides.insert(vid); <- already defined in higher priority decoderInfo (siblings -> variableIds.contains())
        */

        return {};
    };

    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (nodeType(index)) {
        case NodeType::Root:
            break;
        case NodeType::Decoder:
            if (index.column() == 0)
                return decoderData(m_decoders[index.row()], role);

            break;

        case NodeType::Variable:
            return variableData(m_decoders[index.parent().row()], index.row(), index.column(), role);
        }
    }

    return {};
}

quintptr DecoderInfoModel::internalId(NodeType nodeType, const QModelIndex &parent)
{
    return core::value(nodeType) | (static_cast<uint>(parent.row()) << 4);
}

DecoderInfoModel::NodeType DecoderInfoModel::nodeType(const QModelIndex &index)
{
    if (index.isValid())
        return static_cast<NodeType>(index.internalId() & 15);

    return NodeType::Root;
}

int DecoderInfoModel::parentRow(const QModelIndex &index)
{
    return static_cast<int>(index.internalId() >> 4);
}

// =====================================================================================================================

} // namespace

// =====================================================================================================================

class DecoderDatabaseView::Private : public core::PrivateObject<DecoderDatabaseView>
{
public:
    using PrivateObject::PrivateObject;

    auto filteredDecoderModel() const { return static_cast<QSortFilterProxyModel *>(decoderBox->model()); }
    auto decoderInfoModel() const { return static_cast<DecoderInfoModel *>(decoderInfoView->model()); }

    void onCurrentVendorChanged(int index);
    void onCurrentDecoderChanged(int index);
    void onDecoderInfoModelReset();

    core::ConstPointer<QComboBox> vendorBox{q()};
    core::ConstPointer<QComboBox> decoderBox{q()};
    core::ConstPointer<QTreeView> decoderInfoView{q()};
};

void DecoderDatabaseView::Private::onCurrentVendorChanged(int index)
{
    const auto vendorId = vendorBox->itemData(index).toInt();
    auto filter = DecoderTypeModel::vendorFilter(vendorId);
    filteredDecoderModel()->setFilterWildcard(std::move(filter));
}

void DecoderDatabaseView::Private::onCurrentDecoderChanged(int index)
{
    auto decoderId = decoderBox->itemData(index).toString();
    decoderInfoModel()->setDecoderId(std::move(decoderId));
}

void DecoderDatabaseView::Private::onDecoderInfoModelReset()
{
    for (auto row = 0, rowCount = decoderInfoModel()->rowCount({}); row < rowCount; ++row)
        decoderInfoView->setFirstColumnSpanned(row, {}, true);

    decoderInfoView->expandAll();
}

DecoderDatabaseView::DecoderDatabaseView(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    const auto filteredDecoderModel = new QSortFilterProxyModel{this};
    filteredDecoderModel->setSourceModel(new DecoderTypeModel{filteredDecoderModel});
    filteredDecoderModel->setFilterRole(Qt::UserRole);
    filteredDecoderModel->setFilterKeyColumn(0);

    d->vendorBox->setModel(new VendorListModel{d->vendorBox});
    d->decoderBox->setModel(filteredDecoderModel);

    d->decoderInfoView->setHeaderHidden(true);
    d->decoderInfoView->header()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    d->decoderInfoView->setModel(new DecoderInfoModel{this});

    const auto layout = new QFormLayout{this};

    layout->addRow(tr("&Vendor:"), d->vendorBox);
    layout->addRow(tr("&Decoder:"), d->decoderBox);
    layout->addRow(d->decoderInfoView);

    connect(d->vendorBox, &QComboBox::currentIndexChanged, d, &Private::onCurrentVendorChanged);
    connect(d->decoderBox, &QComboBox::currentIndexChanged, d, &Private::onCurrentDecoderChanged);
    connect(d->decoderInfoModel(), &DecoderInfoModel::modelReset, d, &Private::onDecoderInfoModelReset);

    d->onCurrentVendorChanged(d->vendorBox->currentIndex());
    d->onCurrentDecoderChanged(d->decoderBox->currentIndex());
}

} // namespace lmrs::studio

#include "decoderdatabaseview.moc"
