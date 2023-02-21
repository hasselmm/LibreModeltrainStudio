#ifndef SOUNDSLOTTABLEMODEL_H
#define SOUNDSLOTTABLEMODEL_H

#include <QAbstractTableModel>

#include <memory>

namespace esu {
struct MetaData;
} // namespace ecu

class SoundSlotTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum DataColumn {
        NameColumn,
        DescriptionColumn,
        PathColumn,
    };

    Q_ENUM(DataColumn)

    using QAbstractTableModel::QAbstractTableModel;

    void setMetaData(std::shared_ptr<esu::MetaData> metaData);
    std::shared_ptr<esu::MetaData> metaData() const;

public: // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::shared_ptr<esu::MetaData> m_metaData;
};


#endif // SOUNDSLOTTABLEMODEL_H
