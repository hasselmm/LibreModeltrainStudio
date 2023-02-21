#ifndef PACKAGELISTMODEL_H
#define PACKAGELISTMODEL_H

#include <QAbstractListModel>

#include <memory>

namespace esu {
class Package;
} // namespace ecu

class PackageListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum DataRoles {
        FileNameRole = Qt::DisplayRole,
        SizeRole = Qt::UserRole,
        DataRole,
    };

    using QAbstractListModel::QAbstractListModel;

    void setPackage(std::shared_ptr<esu::Package> package);
    std::shared_ptr<esu::Package> package() const;

public: // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    std::shared_ptr<esu::Package> m_package;
};

#endif // PACKAGELISTMODEL_H
