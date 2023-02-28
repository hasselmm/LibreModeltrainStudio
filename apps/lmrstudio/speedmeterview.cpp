#include "speedmeterview.h"

#include "deviceconnectionview.h"

#include <lmrs/core/device.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/widgets/speedmeter.h>

#include <QBoxLayout>
#include <QChart>
#include <QChartView>
#include <QLabel>
#include <QLegendMarker>
#include <QLocale>
#include <QSplineSeries>
#include <QValueAxis>

namespace lmrs::studio {

namespace {

void swapColors(auto s1, auto s2)
{
    auto color = s1->color();
    s1->setColor(s2->color());
    s2->setColor(std::move(color));
}

} // namespace

class SpeedMeterView::Private : public core::PrivateObject<SpeedMeterView>
{
public:
    using PrivateObject::PrivateObject;

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void setMeasuringSpeeds(bool measuringSpeeds);

    void onDataReceived(TimePoint timestamp);
    void onRawSpeedChanged(core::millimeters_per_second speed);
    void onFilteredSpeedChanged(core::millimeters_per_second speed);
    void onPulsesChanged(core::hertz_f pulses);

    QPointer<core::SpeedMeterControl> speedMeter;

    core::ConstPointer<widgets::SpeedMeter> speedGauge{q()};
    core::ConstPointer<QChartView> chartView{q()};
    core::ConstPointer<QValueAxis> speedAxis{chartView->chart()};
    core::ConstPointer<QValueAxis> pulsesAxis{chartView->chart()};
    core::ConstPointer<QValueAxis> accelerationAxis{chartView->chart()};
    core::ConstPointer<QValueAxis> timeAxis{chartView->chart()};
    core::ConstPointer<QSplineSeries> filteredSpeedSeries{chartView->chart()};
    core::ConstPointer<QSplineSeries> rawSpeedSeries{chartView->chart()};
    core::ConstPointer<QSplineSeries> pulseSeries{chartView->chart()};
    core::ConstPointer<QSplineSeries> accelerationSeries{chartView->chart()};

    TimePoint startTime;
    TimePoint previousTime;
    std::chrono::seconds visibleTimeSpan = 2min;
    core::meters_per_second_f previousSpeed = 0;
    core::kilometers_per_hour_f maximumSpeed = 100;
    core::hertz_f maximumPulses = 100;
    core::meters distance = 0;

    QHash<QAbstractSeries *, bool> visibleSeries;
};

void SpeedMeterView::Private::setMeasuringSpeeds(bool measuringSpeeds)
{
    const auto setVisible = [this](auto series, auto visible) {
        series->setVisible(visible && visibleSeries.value(series, true));
    };

    setVisible(accelerationSeries, measuringSpeeds);
    setVisible(filteredSpeedSeries, measuringSpeeds);
    setVisible(rawSpeedSeries, measuringSpeeds);
    setVisible(pulseSeries, !measuringSpeeds);

    accelerationAxis->setVisible(measuringSpeeds);
    pulsesAxis->setVisible(!measuringSpeeds);
    speedAxis->setVisible(measuringSpeeds);
}

void SpeedMeterView::Private::onDataReceived(TimePoint timestamp)
{
    const auto now = std::chrono::duration<double, std::chrono::seconds::period>(timestamp - startTime);
    const auto filteredSpeed = quantityCast<core::kilometers_per_hour_f>(speedMeter->filteredSpeed());
    const auto rawSpeed = quantityCast<core::kilometers_per_hour_f>(speedMeter->rawSpeed());
    const auto pulses = speedMeter->pulses();

    filteredSpeedSeries->append(now.count(), filteredSpeed.count());
    rawSpeedSeries->append(now.count(), rawSpeed.count());
    pulseSeries->append(now.count(), pulses.count());

    distance += rawSpeed * 1s;

    using seconds_f = std::chrono::duration<qreal, std::chrono::seconds::period>; // FIXME: move to quantities

    if (const auto dt = seconds_f{timestamp - previousTime}; dt > 0s) {
        const auto metersPerSecond = quantityCast<core::meters_per_second_f>(speedMeter->filteredSpeed());
        const auto acceleration = (metersPerSecond - previousSpeed) / dt;

        previousSpeed = metersPerSecond;
        previousTime = timestamp;

        if (filteredSpeedSeries->count() >= 2) {
            accelerationSeries->append(now.count(), acceleration.count());
            qInfo() << "acceleration:" << acceleration << dt;

            if (qAbs(acceleration.count()) > accelerationAxis->max()
                    || -qAbs(acceleration.count()) < accelerationAxis->min()) {
                const auto limit = qCeil((qAbs(acceleration.count()) + 0.5) / 0.5) * 0.5;
                accelerationAxis->setRange(-limit, +limit);
            }
        }
    }

    const auto dt = static_cast<int>(visibleTimeSpan.count());
    const auto min = qMax(0, qFloor(now.count() - dt));
    timeAxis->setRange(min, min + dt);
}

void SpeedMeterView::Private::onRawSpeedChanged(core::millimeters_per_second speed)
{
    const auto kilometersPerHour = quantityCast<core::kilometers_per_hour_f>(speed);
    speedGauge->setValue(kilometersPerHour.count());
    setMeasuringSpeeds(true);

    if (std::exchange(maximumSpeed, std::max(maximumSpeed, kilometersPerHour)) != maximumSpeed) {
        speedAxis->setRange(0, qCeil((maximumSpeed.count() + 25) / 50) * 50);
        speedGauge->setMaximumValue(maximumSpeed.count());
    }
}

void SpeedMeterView::Private::onFilteredSpeedChanged(core::millimeters_per_second speed)
{
    const auto kilometersPerHour = quantityCast<core::kilometers_per_hour_f>(speed);
    const auto metersPerSecond = quantityCast<core::meters_per_second_f>(speed);

    speedGauge->setTopText(toString(kilometersPerHour) + '\n'_L1 + toString(metersPerSecond) + '\n'_L1);

    if (distance < 5_km)
        speedGauge->setBottomText(toString(distance));
    else
        speedGauge->setBottomText(toString(quantityCast<core::kilometers_f>(distance)));

    setMeasuringSpeeds(true);
}

void SpeedMeterView::Private::onPulsesChanged(core::hertz_f pulses)
{
    if (pulseSeries->isVisible()) {
        speedGauge->setValue(pulses.count());
        speedGauge->setBottomText(toString(pulses));
    }

    if (std::exchange(maximumPulses, std::max(maximumPulses, pulses)) != maximumPulses) {
        pulsesAxis->setRange(0, qCeil((maximumPulses.count() + 25) / 50) * 50);

        if (pulseSeries->isVisible())
            speedGauge->setMaximumValue(maximumPulses.count());
    }
}

SpeedMeterView::SpeedMeterView(QWidget *parent)
    : MainWindowView{parent}
    , d{new Private{this}}
{
    setEnabled(false);

    d->speedGauge->setMaximumValue(d->maximumSpeed.count());
    d->speedGauge->setMeasurementInterval(1s);
    d->speedGauge->setMinimumSize(100, 100);
    d->speedGauge->setMaximumSize(320, 320);

    d->chartView->setRenderHint(QPainter::RenderHint::Antialiasing);

    const auto chart = d->chartView->chart();

    chart->addAxis(d->speedAxis, Qt::AlignLeft);
    chart->addAxis(d->pulsesAxis, Qt::AlignLeft);
    chart->addAxis(d->accelerationAxis, Qt::AlignRight);
    chart->addAxis(d->timeAxis, Qt::AlignBottom);

    chart->addSeries(d->accelerationSeries);
    chart->addSeries(d->filteredSpeedSeries);
    chart->addSeries(d->rawSpeedSeries);
    chart->addSeries(d->pulseSeries);

    // draw speed series over acceleration series, but keep orange color for acceleration series
    swapColors(d->accelerationSeries, d->rawSpeedSeries);

    // pulses and raw speed basically are the same thing, just at different scale
    d->pulseSeries->setColor(d->rawSpeedSeries->color());

    for (auto marker: chart->legend()->markers()) {
        connect(marker, &QLegendMarker::clicked, this, [this, marker] {
            const auto series = marker->series();
            series->setVisible(!series->isVisible());
            d->visibleSeries[series] = series->isVisible();
            marker->setVisible(true);

            auto color = marker->labelBrush().color();
            color.setAlphaF(series->isVisible() ? 1.0f : 0.6f);
            marker->setLabelBrush(std::move(color));

            color = marker->brush().color();
            color.setAlphaF(series->isVisible() ? 1.0f : 0.2f);
            marker->setBrush(std::move(color));

            color = marker->pen().color();
            color.setAlphaF(series->isVisible() ? 1.0f : 0.2f);
            marker->setPen(std::move(color));
        });
    }

    chart->setMargins({});
    chart->setBackgroundRoundness(0);
    chart->setPlotAreaBackgroundBrush(chart->backgroundBrush());
    chart->setBackgroundBrush(Qt::transparent);
    chart->setPlotAreaBackgroundVisible();

    d->speedAxis->setRange(0, d->maximumSpeed.count());
    d->speedAxis->setLabelFormat("%d"_L1);

    d->pulsesAxis->setRange(0, d->maximumPulses.count());
    d->pulsesAxis->setLabelFormat("%d"_L1);

    d->timeAxis->setRange(0, static_cast<int>(d->visibleTimeSpan.count()));
    d->timeAxis->setLabelFormat("%d"_L1);

    d->accelerationAxis->setRange(-1.5f, +1.5f);
    d->timeAxis->setLabelFormat("%.1f"_L1);

    d->rawSpeedSeries->setName(tr("Raw Speed (km/h)"));
    d->rawSpeedSeries->attachAxis(d->speedAxis);
    d->rawSpeedSeries->attachAxis(d->timeAxis);

    d->filteredSpeedSeries->setName(tr("Filtered Speed (km/h)"));
    d->filteredSpeedSeries->attachAxis(d->speedAxis);
    d->filteredSpeedSeries->attachAxis(d->timeAxis);

    d->pulseSeries->setName(tr("Raw Pulses (hz)"));
    d->pulseSeries->attachAxis(d->pulsesAxis);
    d->pulseSeries->attachAxis(d->timeAxis);

    d->accelerationSeries->setName(tr("Acceleration (m/s²)"));
    d->accelerationSeries->attachAxis(d->accelerationAxis);
    d->accelerationSeries->attachAxis(d->timeAxis);

    d->setMeasuringSpeeds(false);

    const auto layout = new QVBoxLayout{this};

    layout->addWidget(d->speedGauge);
    layout->setAlignment(d->speedGauge, Qt::AlignHCenter);
    layout->addWidget(d->chartView);
}

DeviceFilter SpeedMeterView::deviceFilter() const
{
    return DeviceFilter::require<core::SpeedMeterControl>();
}

void SpeedMeterView::updateControls(core::Device *newDevice)
{
    setSpeedMeter(newDevice ? newDevice->speedMeterControl() : nullptr);
}

void SpeedMeterView::setSpeedMeter(core::SpeedMeterControl *newSpeedMeter)
{
    if (const auto oldSpeedMeter = std::exchange(d->speedMeter, newSpeedMeter); d->speedMeter != oldSpeedMeter) {
        if (oldSpeedMeter)
            oldSpeedMeter->disconnect(d);

        if (newSpeedMeter) {
            connect(newSpeedMeter, &core::SpeedMeterControl::dataReceived, d, &Private::onDataReceived);
            connect(newSpeedMeter, &core::SpeedMeterControl::pulsesChanged, d, &Private::onPulsesChanged);
            connect(newSpeedMeter, &core::SpeedMeterControl::rawSpeedChanged, d, &Private::onRawSpeedChanged);
            connect(newSpeedMeter, &core::SpeedMeterControl::filteredSpeedChanged, d, &Private::onFilteredSpeedChanged);
        }

        for (auto series: d->chartView->chart()->series())
            static_cast<QXYSeries *>(series)->clear();

        d->startTime = Private::Clock::now();
        d->previousTime = d->startTime;
        d->previousSpeed = {};
        d->distance = {};

        d->speedGauge->setValue(0);
        d->speedGauge->setTopText({});
        d->speedGauge->setBottomText({});

        d->visibleSeries.clear();
        d->setMeasuringSpeeds(newSpeedMeter && newSpeedMeter->hasFeature(core::SpeedMeterControl::Feature::MeasureSpeed));
    }

    setEnabled(!d->speedMeter.isNull());
}

core::SpeedMeterControl *SpeedMeterView::speedMeter() const
{
    return d->speedMeter;
}

} // lmrs::studio

#include "moc_speedmeterview.cpp"
