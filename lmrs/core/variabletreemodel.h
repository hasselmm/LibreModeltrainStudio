#ifndef LMRS_CORE_VARIABLETREEMODEL_H
#define LMRS_CORE_VARIABLETREEMODEL_H

#include "dccconstants.h"

#include <QAbstractItemModel>
#include <QJsonObject>

namespace lmrs::core::dcc {
class DecoderVariable;
} // namespace lmrs::core::dcc

namespace lmrs::core {

class VariableModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column { Index, Name, Bitmask, Value, Description };

    using QAbstractItemModel::QAbstractItemModel;

    QString decoderId() const;
    QList<dcc::ExtendedVariableIndex> knownVariables() const; // FIXME: use dcc type

    void updateVariable(dcc::ExtendedVariableIndex variable, dcc::VariableValue value); // FIXME: use dcc types
    dcc::DecoderVariable variableInfo(dcc::ExtendedVariableIndex index) const; // FIXME: use dcc types
    dcc::VariableValue variableValue(dcc::ExtendedVariableIndex variable) const; // FIXME: use dcc types

    void clear();

public: // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    bool hasChildren(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

signals:
    void variableChanged(lmrs::core::dcc::ExtendedVariableIndex variable,
                         lmrs::core::dcc::VariableValue value);

private:
    void emitDataChanges(int row);

    struct Row {
        enum Flag {
            HasValue16  = (1 << 0),
            IsHighByte  = (1 << 1),
            IsLowByte   = (1 << 2),

            HasValue32  = (1 << 3),
            IsByteA     = (1 << 4),
            IsByteB     = (1 << 5),
            IsByteC     = (1 << 6),
            IsByteD     = (1 << 7),

            HasFields   = (1 << 8),
            HasFlags    = (1 << 9),
            HasChildren = HasFields | HasFlags,

            HasFieldsWithFlags = (1 << 12),
            HasGrandChildren = HasFieldsWithFlags,
        };

        Q_DECLARE_FLAGS(Flags, Flag)

        QVariant data(QModelIndex index, int role) const;
        QVariant childData(QModelIndex index, int role) const;
        QVariant grandChildData(QModelIndex index, int role) const;
        QVariant fieldData(QModelIndex index, int role, dcc::DecoderVariable variable) const;
        QString description(dcc::DecoderVariable variable) const;

        static QVariant flagData(QModelIndex index, int role, QJsonArray flags, dcc::VariableValue value);

        dcc::ExtendedVariableIndex variable;
        Flags flags;

        quint8 value8 = 0;
        quint16 value16 = 0;
        quint32 value32 = 0;
    };

    QList<Row> m_rows;
    QJsonObject m_variableInfo;
};

} // namespace lmrs::core

#endif // LMRS_CORE_VARIABLETREEMODEL_H
