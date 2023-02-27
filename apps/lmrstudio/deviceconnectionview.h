#ifndef LMRS_STUDIO_DEVICECONNECTIONVIEW_H
#define LMRS_STUDIO_DEVICECONNECTIONVIEW_H

#include <lmrs/core/device.h>

#include <QWidget>

namespace lmrs::studio {

class DeviceParameterWidget;

class DeviceModelInterface
{
    Q_GADGET

public:
    enum class Role {
        Name = Qt::DisplayRole,
        Icon = Qt::DecorationRole,
        Device = Qt::UserRole,
        Features,
    };

    static core::Device *device(QModelIndex index);
    QModelIndex findDevice(QString uniqueId) const;
    QModelIndex indexOf(const core::Device *device) const;

protected:
    virtual const QAbstractItemModel *model() const = 0;
};

class DeviceConnectionView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(lmrs::core::Device *currentDevice READ currentDevice NOTIFY currentDeviceChanged FINAL)

public:
    explicit DeviceConnectionView(QWidget *parent = nullptr);
    ~DeviceConnectionView() override;

    using DeviceFilter = bool (*)(const core::Device *);
    QAbstractItemModel *model(DeviceFilter filter) const;
    template<class T> QAbstractItemModel *model() const;
    QAbstractItemModel *model() const;

    core::Device *currentDevice() const;
    bool hasAcceptableInput() const;

    QList<DeviceParameterWidget *> widgets() const;
    void setCurrentWidget(DeviceParameterWidget *widget);
    DeviceParameterWidget *currentWidget() const;

    void storeSettings();
    void restoreSettings();

public slots:
    void connectToDevice();
    void disconnectFromDevice();

signals:
    void currentDeviceChanged(lmrs::core::Device *currentDevice);
    void currentWidgetChanged();

private:

    class Private;
    Private *const d;
};

template<class T>
QAbstractItemModel *DeviceConnectionView::model() const
{
    return model([](const core::Device *device) {
        return device && device->control<T>() != nullptr;
    });
}

} // namespace lmrs::studio

#endif // LMRS_STUDIO_DEVICECONNECTIONVIEW_H
