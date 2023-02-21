#ifndef LMRS_WIDGETS_SPINBOX_H
#define LMRS_WIDGETS_SPINBOX_H

#include <QSpinBox>

#include <lmrs/core/typetraits.h>

class QActionGroup;

namespace lmrs::widgets {

class MultiBaseSpinBox : public QSpinBox
{
public:
    explicit MultiBaseSpinBox(QWidget *parent = nullptr);

protected: // QAbstractSpinBox interface
    QValidator::State validate(QString &input, int &pos) const override;
    int valueFromText(const QString &text) const override;
    QString textFromValue(int value) const override;

protected: // QWidget interface
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void updateInputMask();

    QActionGroup *const m_baseActionGroup;
};

template<typename T>
class SpinBox : public QSpinBox
{
public:
    explicit SpinBox(QWidget *parent)
        : QSpinBox{parent}
    {
        setRange(core::MinimumValue<T>, core::MaximumValue<T>);
    }

    void setValue(T value) { QSpinBox::setValue(value); }
    T value() const { return static_cast<typename T::value_type>(QSpinBox::value()); }

    void setMinimum(T minimum) { QSpinBox::setMinimum(minimum); }
    T minimum() const { return static_cast<typename T::value_type>(QSpinBox::minimum()); }

    void setMaximum(T maximum) { QSpinBox::setMaximum(maximum); }
    T maximum() const { return static_cast<typename T::value_type>(QSpinBox::maximum()); }

    void setRange(T minimum, T maximum) { QSpinBox::setRange(minimum, maximum); }
};

} // namespace lmrs::widgets

#endif // LMRS_WIDGETS_SPINBOX_H
