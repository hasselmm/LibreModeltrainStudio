#include "soundslottablemodel.h"

#include <lmrs/core/algorithms.h>

#include "metadata.h"

#include <QMetaEnum>

namespace {

using namespace lmrs;

constexpr auto DefaultSlotCount = 34; // FIXME: figure out proper maximal count of slots
constexpr auto SoundSlotBreaking = 33; // FIXME: make a proper enum class or LiteralType
constexpr auto SoundSlotShifting = 34; // FIXME: make a proper enum class or LiteralType

auto slotName(int id)
{
    if (id == SoundSlotBreaking)
        return SoundSlotTableModel::tr("Breaking");
    else if (id == SoundSlotShifting)
        return SoundSlotTableModel::tr("Shifting");
    else
        return SoundSlotTableModel::tr("Slot %1").arg(id);
}

}

void SoundSlotTableModel::setMetaData(std::shared_ptr<esu::MetaData> metaData)
{
    if (metaData != m_metaData) {
        beginResetModel();
        std::swap(m_metaData, metaData);
        endResetModel();
    }

}

std::shared_ptr<esu::MetaData> SoundSlotTableModel::metaData() const
{
    return m_metaData;
}

int SoundSlotTableModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(!m_metaData))
        return 0;
    if (Q_UNLIKELY(parent.isValid()))
        return 0;
    if (Q_UNLIKELY(m_metaData->soundSlots.isEmpty()))
        return 0;

    return qMax(DefaultSlotCount, m_metaData->soundSlots.lastKey());
}

int SoundSlotTableModel::columnCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return core::keyCount<DataColumn>();
}

QVariant SoundSlotTableModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        const auto slotId = index.row() + 1;

        switch (static_cast<DataColumn>(index.column())) {
        case NameColumn:
            if (role == Qt::DisplayRole)
                return slotName(slotId);

            break;

        case DescriptionColumn:
            if (role == Qt::DisplayRole)
                return m_metaData->soundSlots.value(slotId).description.toString();

            break;

        case PathColumn:
            if (role == Qt::DisplayRole)
                return m_metaData->soundSlots.value(slotId).path;

            break;
        }
    }

    return {};
}

QVariant SoundSlotTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        switch (static_cast<DataColumn>(section)) {
        case NameColumn:
            if (role == Qt::DisplayRole)
                return tr("Function");

            break;

        case DescriptionColumn:
            if (role == Qt::DisplayRole)
                return tr("Description");

            break;

        case PathColumn:
            if (role == Qt::DisplayRole)
                return tr("Path");

            break;
        }
    }

    return {};
}
