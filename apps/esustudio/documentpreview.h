#ifndef DOCUMENTPREVIEW_H
#define DOCUMENTPREVIEW_H

#include <QWidget>

class DocumentPreview : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentPreview(QWidget *parent = {});
    ~DocumentPreview() override;

    bool load(QByteArray data);

private:
    class Private;
    Private *const d;
};

#endif // DOCUMENTPREVIEW_H
