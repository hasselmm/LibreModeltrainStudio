#ifndef LMRS_STUDIO_FUNCTIONMAPPINGVIEW_H
#define LMRS_STUDIO_FUNCTIONMAPPINGVIEW_H

#include <lmrs/core/device.h>

#include <QTableView>

namespace lmrs::core {
class VariableControl;
} // namespace lmrs::core

namespace lmrs::studio {

class FunctionMappingView : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionMappingView(QAbstractItemModel *devices, QWidget *parent = {});

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_FUNCTIONMAPPINGVIEW_H
