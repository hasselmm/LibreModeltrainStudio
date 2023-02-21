#ifndef AUDIOPREVIEW_H
#define AUDIOPREVIEW_H

#include <QWidget>

class AudioPreview : public QWidget
{
    Q_OBJECT

public:
    explicit AudioPreview(QWidget *parent = nullptr);
    ~AudioPreview() override;

    bool load(QByteArray data);

public slots:
    void play();
    void stop();

signals:
    void started();
    void finished();

private:
    class Private;
    Private *const d;
};

#endif // AUDIOPREVIEW_H
