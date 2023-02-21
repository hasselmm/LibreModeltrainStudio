#include "speeddial.h"

#include <lmrs/core/propertyguard.h>

#include <QBoxLayout>
#include <QDial>

namespace lmrs::widgets {

class SpeedDial::Private : public QDial
{
public:
    using QDial::QDial;

    SpeedDial *speedDial() const { return static_cast<SpeedDial *>(parent()); }
    Bias bias() const;

    void reset(int newCenterSteps, int newValueSteps, int newEndSteps);
    void setBias(Bias bias);

    void onValueChanged(int value);

    int centerSteps = 0;
    int valueSteps = maximum();
    int endSteps = 0;

    int normalizedValue = value();
};

SpeedDial::Bias SpeedDial::Private::bias() const
{
    return naturalBias(QDial::value());
}

void SpeedDial::Private::reset(int newCenterSteps, int newValueSteps, int newEndSteps)
{
    const auto oldBias = bias();
    const auto oldValue = value();

    centerSteps = newCenterSteps;
    valueSteps = newValueSteps;
    endSteps = newEndSteps;

    const auto valueRange = centerSteps + valueSteps + endSteps;

    setRange(-valueRange, +valueRange);
    speedDial()->setValue(oldValue, oldBias);
}

void SpeedDial::Private::setBias(Bias bias)
{
    if (centerSteps > 0) {
        switch (bias) {
        case Bias::Negative:
            QDial::setValue(-qMax(1, abs(value())));
            break;

        case Bias::Positive:
            QDial::setValue(+qMax(1, abs(value())));
            break;

        case Bias::Neutral:
            break;
        }
    }
}

void SpeedDial::Private::onValueChanged(int value)
{
    const auto biasGuard = core::propertyGuard(speedDial(), &SpeedDial::bias, &SpeedDial::biasChanged);
    const auto valueGuard = core::propertyGuard(speedDial(), &SpeedDial::value, &SpeedDial::valueChanged);
    const auto valueBlocker = QSignalBlocker{this};

    const auto positiveLimit = +(centerSteps + valueSteps);
    const auto negativeLimit = -(centerSteps + valueSteps);

    const auto initialBias = bias();

    if (value == 0) {
        QDial::setValue(0); // avoid division by zero in next branch
    } else if (abs(value) <= centerSteps) {
        QDial::setValue(value / abs(value));
    } else if (value >= positiveLimit) {
        QDial::setValue(positiveLimit);
    } else if (value <= negativeLimit) {
        QDial::setValue(negativeLimit);
    }

    normalizedValue = qBound(0, abs(value) - centerSteps, valueSteps - 1) * (value < 0 ? -1 : 1);

    if (normalizedValue == 0)
        setBias(initialBias);
}

SpeedDial::SpeedDial(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    d->reset(20, 100, 10);

    const auto layout = new QVBoxLayout{this};
    layout->addWidget(d);

    connect(d, &QDial::valueChanged, d, &Private::onValueChanged);
}

void SpeedDial::setCenterSteps(int centerSteps)
{
    d->reset(centerSteps, valueSteps(), endSteps());
}

int SpeedDial::centerSteps() const
{
    return d->centerSteps;
}

void SpeedDial::setValueSteps(int valueSteps)
{
    d->reset(centerSteps(), valueSteps, endSteps());
}

int SpeedDial::valueSteps() const
{
    return d->valueSteps;
}

void SpeedDial::setEndSteps(int endSteps)
{
    d->reset(centerSteps(), valueSteps(), endSteps);
}

int SpeedDial::endSteps() const
{
    return d->endSteps;
}

void SpeedDial::setValue(int value, Bias bias)
{
    const auto biasGuard = core::propertyGuard(this, &SpeedDial::bias, &SpeedDial::biasChanged);
    const auto valueGuard = core::propertyGuard(this, &SpeedDial::value, &SpeedDial::valueChanged);
    const auto valueBlocker = QSignalBlocker{this};

    if (bias == Bias::Neutral)
        bias = naturalBias(value);

    if (value != 0) {
        const auto normalizedValue = d->centerSteps + abs(value);
        const auto maximumValue = d->centerSteps + d->valueSteps;
        d->setValue(qMin(normalizedValue, maximumValue));
    } else {
        d->setValue(0);
    }

    d->setBias(bias);
}

int SpeedDial::value() const
{
    return d->normalizedValue;
}

void SpeedDial::setBias(Bias bias)
{
    d->setBias(bias);
}

SpeedDial::Bias SpeedDial::bias() const
{
    return d->bias();
}

SpeedDial::Bias SpeedDial::naturalBias(int value)
{
    if (value < 0)
        return Bias::Negative;
    if (value > 0)
        return Bias::Positive;

    return Bias::Neutral;
}

} // namespace lmrs::widgets
