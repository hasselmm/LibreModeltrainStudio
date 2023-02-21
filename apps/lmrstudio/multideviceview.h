#ifndef LMRS_STUDIO_MULTIDEVICEVIEW_H
#define LMRS_STUDIO_MULTIDEVICEVIEW_H

#include <QFrame>

class QAbstractItemModel;

namespace lmrs::core {
class Device;
}

namespace lmrs::studio {

class AbstractMultiDeviceView : public QFrame
{
    Q_OBJECT

public:
    explicit AbstractMultiDeviceView(QWidget *parent = nullptr);
    explicit AbstractMultiDeviceView(QAbstractItemModel *model, QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

protected:
    virtual QWidget *createWidget(core::Device *device, QWidget *parent) = 0;

private:
    class Private;
    Private *const d;
};

template<class WidgetType = QWidget>
class MultiDeviceView : public AbstractMultiDeviceView
{
public:
    using AbstractMultiDeviceView::AbstractMultiDeviceView;

protected:
    QWidget *createWidget(core::Device *device, QWidget *parent) override;
};

template<class WidgetType>
inline QWidget *MultiDeviceView<WidgetType>::createWidget(core::Device *device, QWidget *parent)
{
    const auto widget = new WidgetType{parent};
    widget->setDevice(device);
    return widget;
}

} // namespace lmrs::studio

#endif // LMRS_STUDIO_MULTIDEVICEVIEW_H
