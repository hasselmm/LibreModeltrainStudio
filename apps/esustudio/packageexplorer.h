#ifndef PACKAGEEXPLORER_H
#define PACKAGEEXPLORER_H

#include <QWidget>

namespace esu {
class Package;
} // namespace ecu

class PackageExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit PackageExplorer(QWidget *parent = {});
    ~PackageExplorer() override;

    void setPackage(std::shared_ptr<esu::Package> package);
    std::shared_ptr<esu::Package> package() const;

private:
    class Private;
    Private *const d;
};

#endif // PACKAGEEXPLORER_H
