#ifndef IMAGEPREVIEW_H
#define IMAGEPREVIEW_H

#include <QWidget>

class QLabel;

class ImagePreview : public QWidget
{
    Q_OBJECT

public:
    explicit ImagePreview(QWidget *parent = nullptr);

    bool load(QByteArray data);

private:
    QLabel *const m_imageLabel;
    QLabel *const m_infoLabel;
};

#endif // IMAGEPREVIEW_H
