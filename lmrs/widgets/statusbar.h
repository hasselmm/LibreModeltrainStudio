#ifndef LMRS_WIDGETS_STATUSBAR_H
#define LMRS_WIDGETS_STATUSBAR_H

#include <QStatusBar>

namespace lmrs::widgets {

class StatusBar : public QStatusBar
{
    Q_OBJECT

public:
    static constexpr std::chrono::milliseconds DefaultTimeout = std::chrono::seconds{5};

    explicit StatusBar(QWidget *parent = nullptr);

    void clearPermanentMessage();
    void showPermanentMessage(QString message);
    void showTemporaryMessage(QString message, std::chrono::milliseconds timeout = DefaultTimeout);


private:
    QString m_permanentMessage;
};

} // namespace widgets::widgets

#endif // LMRS_WIDGETS_STATUSBAR_H
