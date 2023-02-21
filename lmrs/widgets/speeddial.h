#ifndef LMRS_WIDGETS_SPEEDDIAL_H
#define LMRS_WIDGETS_SPEEDDIAL_H

#include <QWidget>

namespace lmrs::widgets {

class SpeedDial : public QWidget
{
    Q_OBJECT

public:
    enum class Bias { Negative = -1, Neutral, Positive };

    Q_ENUM(Bias)

    explicit SpeedDial(QWidget *parent = nullptr);

    void setCenterSteps(int centerSteps);
    int centerSteps() const;

    void setValueSteps(int valueSteps);
    int valueSteps() const;

    void setEndSteps(int endSteps);
    int endSteps() const;

    void setValue(int value, Bias bias = Bias::Neutral);
    int value() const;

    void setBias(Bias bias);
    Bias bias() const;

    static Bias naturalBias(int value);

signals:
    void valueChanged(int value);
    void biasChanged(Bias bias);

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_SPEEDDIAL_H
