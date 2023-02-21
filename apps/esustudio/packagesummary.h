#ifndef PACKAGESUMMARY_H
#define PACKAGESUMMARY_H

#include <QWidget>

namespace esu {
struct MetaData;
} // namespace ecu

class PackageSummary : public QWidget
{
    Q_OBJECT

public:
    explicit PackageSummary(QWidget *parent = nullptr);

    void setMetaData(std::shared_ptr<esu::MetaData> metaData);
    std::shared_ptr<esu::MetaData> metaData() const;

private:
    class Private;
    Private *const d;
};

#endif // PACKAGESUMMARY_H
