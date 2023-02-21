#include "imagepreview.h"

#include <QBoxLayout>
#include <QImage>
#include <QLabel>

ImagePreview::ImagePreview(QWidget *parent)
    : QWidget{parent}
    , m_imageLabel{new QLabel{this}}
    , m_infoLabel{new QLabel{this}}
{
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setAlignment(Qt::AlignCenter);

    const auto layout = new QVBoxLayout{this};

    layout->addStretch();
    layout->addWidget(m_imageLabel);
    layout->addWidget(m_infoLabel);
    layout->addStretch();
}

bool ImagePreview::load(QByteArray data)
{
    auto image = QImage::fromData(std::move(data));

    if (image.isNull()) {
        m_imageLabel->setPixmap({});
        return false;
    }

    m_infoLabel->setText(tr("%1 x %2").arg(image.width()).arg(image.height()));
    m_imageLabel->setPixmap(QPixmap::fromImage(std::move(image)));

    return true;
}
