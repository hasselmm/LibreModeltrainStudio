#ifndef LMRS_WIDGETS_SPEEDMETER_H
#define LMRS_WIDGETS_SPEEDMETER_H

#include <QWidget>

namespace lmrs::widgets {

class SpeedMeter : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedMeter(QWidget *parent = nullptr);

    void setValue(qreal value);
    qreal value() const;

    void setMaximumValue(qreal maximumValue);
    qreal maximumValue() const;

    void setMeasurementInterval(std::chrono::milliseconds interval);
    std::chrono::milliseconds measurementInterval() const;

    void setTopText(QString text);
    QString topText() const;

    void setBottomText(QString text);
    QString bottomText() const;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_SPEEDMETER_H
