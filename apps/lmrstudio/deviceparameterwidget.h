#ifndef LMRS_STUDIO_DEVICEPARAMETERWIDGET_H
#define LMRS_STUDIO_DEVICEPARAMETERWIDGET_H

#include <QWidget>

namespace lmrs::core {
class DeviceFactory;
}

namespace lmrs::studio {

class DeviceParameterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceParameterWidget(core::DeviceFactory *factory, QWidget *parent = nullptr);

    core::DeviceFactory *factory() const;

    void setParameters(QVariantMap parameters);
    QVariantMap parameters() const;

    bool hasAcceptableInput() const;

signals:
    void activated();
    void hasAcceptableInputChanged();

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_DEVICEPARAMETERWIDGET_H
