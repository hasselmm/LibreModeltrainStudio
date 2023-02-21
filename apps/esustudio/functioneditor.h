#ifndef FUNCTIONEDITOR_H
#define FUNCTIONEDITOR_H

#include <QWidget>

namespace esu {
struct MetaData;
} // namespace ecu

class FunctionEditor : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionEditor(QWidget *parent = nullptr);

    void setMetaData(std::shared_ptr<esu::MetaData> metaData);
    std::shared_ptr<esu::MetaData> metaData() const;

private:
    class Private;
    Private *const d;
};

#endif // FUNCTIONEDITOR_H
