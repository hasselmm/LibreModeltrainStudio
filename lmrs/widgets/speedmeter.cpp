#include "speedmeter.h"

#include <lmrs/core/staticinit.h>
#include <lmrs/core/userliterals.h>

#include <QBoxLayout>
#include <QQuickItem>
#include <QQuickWidget>

static void initResources() { Q_INIT_RESOURCE(widgets); }

namespace lmrs::widgets {

class SpeedMeter::Private
        : public core::StaticInit<Private>
        , public QQuickWidget
{
public:
    using QQuickWidget::QQuickWidget;
    static void staticConstructor();
};

void SpeedMeter::Private::staticConstructor()
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
//    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
}

SpeedMeter::SpeedMeter(QWidget *parent)
    : QWidget{parent}
    , d{new Private{"qrc:/taschenorakel.de/lmrs/widgets/qml/SpeedMeter.qml"_url, this}}
{
    initResources();

    auto surfaceFormat = d->format();
    surfaceFormat.setAlphaBufferSize(8);
    d->setFormat(std::move(surfaceFormat));

    d->setAttribute(Qt::WA_AlwaysStackOnTop);
    d->setAttribute(Qt::WA_TranslucentBackground);
    d->setClearColor(Qt::transparent);
    d->setResizeMode(QQuickWidget::ResizeMode::SizeRootObjectToView);

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(d);
}

void SpeedMeter::setTopText(QString text)
{
    d->rootObject()->setProperty("topText", std::move(text));
}

void SpeedMeter::setBottomText(QString text)
{
    d->rootObject()->setProperty("bottomText", std::move(text));
}

void SpeedMeter::setValue(qreal value)
{
    d->rootObject()->setProperty("value", value);
}

void SpeedMeter::setMaximumValue(qreal maximumValue)
{
    d->rootObject()->setProperty("maximumValue", maximumValue);
}

void SpeedMeter::setMeasurementInterval(std::chrono::milliseconds interval)
{
    d->rootObject()->setProperty("measurementInterval", static_cast<int>(interval.count()));
}

} // namespace lmrs::widgets
