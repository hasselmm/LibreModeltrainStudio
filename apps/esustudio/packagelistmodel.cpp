#include "packagelistmodel.h"

#include "package.h"

void PackageListModel::setPackage(std::shared_ptr<esu::Package> package)
{
    if (package != m_package) {
        beginResetModel();
        std::swap(m_package, package); // FIXME: opening/closing
        endResetModel();
    }
}

std::shared_ptr<esu::Package> PackageListModel::package() const
{
    return m_package;
}

int PackageListModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;
    if (Q_UNLIKELY(!m_package))
        return 0;

    return static_cast<int>(m_package->fileCount());
}

QVariant PackageListModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        switch (static_cast<DataRoles>(role)) {
        case FileNameRole:
            return m_package->fileName(index.row());
        case SizeRole:
            return m_package->size(index.row());
        case DataRole:
            return m_package->data(index.row());
        }
    }

    return {};
}
