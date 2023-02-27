#ifndef LMRS_STUDIO_DEVICECONNECTIONVIEW_H
#define LMRS_STUDIO_DEVICECONNECTIONVIEW_H

#include <lmrs/core/device.h>

#include <QWidget>

namespace lmrs::studio {

class DeviceParameterWidget;

class DeviceFilter
{
public:
    using Function = bool (*)(const core::Device *) noexcept;
    using Key = const void *;

    constexpr DeviceFilter(Function function) noexcept
        : m_function{(function ? function : any())}
    {}

    constexpr bool operator()(const core::Device *device) const noexcept { return m_function(device); }
    auto key() const { return reinterpret_cast<Key>(m_function); }

    template<class T>
    struct Convertable
    {
        constexpr operator Function() const { return &T::invoke; }
        constexpr operator DeviceFilter() const { return {&T::invoke}; }
    };

    struct Any : public Convertable<Any>
    {
        static constexpr bool invoke(const core::Device *) noexcept { return true; }
    };

    struct None : public Convertable<None>
    {
        static constexpr bool invoke(const core::Device *) noexcept { return false; }
    };

    template<class T>
    struct Require : public Convertable<Require<T>>
    {
        static constexpr bool invoke(const core::Device *device) noexcept {
            return device && device->control<T>() != nullptr;
        };
    };

    static constexpr Any any() noexcept { return {}; }
    static constexpr None none() noexcept { return {}; }

    template<class T>
    static constexpr Require<T> require() noexcept { return {}; }

private:
    Function m_function = any();
};

template<typename... Filters>
static constexpr DeviceFilter::Function acceptAny() noexcept {
    return [](const core::Device *device) noexcept {
        return (... || Filters::invoke(device));
    };
}

template<typename... Filters>
static constexpr DeviceFilter::Function requireAll() noexcept {
    return [](const core::Device *device) noexcept {
        return (... && Filters::invoke(device));
    };
}

static_assert(std::invoke(DeviceFilter::any(), nullptr) == true);
static_assert(std::invoke(DeviceFilter::none(), nullptr) == false);
static_assert(std::invoke(acceptAny<DeviceFilter::Any, DeviceFilter::Any>(), nullptr) == true);
static_assert(std::invoke(acceptAny<DeviceFilter::Any, DeviceFilter::None>(), nullptr) == true);
static_assert(std::invoke(acceptAny<DeviceFilter::None, DeviceFilter::None>(), nullptr) == false);
static_assert(std::invoke(requireAll<DeviceFilter::Any, DeviceFilter::Any>(), nullptr) == true);
static_assert(std::invoke(requireAll<DeviceFilter::Any, DeviceFilter::None>(), nullptr) == false);
static_assert(std::invoke(requireAll<DeviceFilter::None, DeviceFilter::None>(), nullptr) == false);

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

    QAbstractItemModel *model(DeviceFilter filter) const;
    template<class T> QAbstractItemModel *model() const { return model(DeviceFilter::require<T>()); }

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
    void currentDeviceChanged(lmrs::core::Device *currentDevice, QPrivateSignal);
    void currentWidgetChanged();

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_DEVICECONNECTIONVIEW_H
