#include "statusbar.h"

namespace lmrs::widgets {

StatusBar::StatusBar(QWidget *parent)
    : QStatusBar{parent}
{
    connect(this, &StatusBar::messageChanged, this, [this](QString message) {
        if (message.isEmpty())
            QStatusBar::showMessage(m_permanentMessage);
    });
}

void StatusBar::clearPermanentMessage()
{
    m_permanentMessage.clear();
}

void StatusBar::showPermanentMessage(QString message)
{
    if (message.isEmpty())
        return;

    m_permanentMessage = std::move(message);
    QStatusBar::showMessage(m_permanentMessage);
}

void StatusBar::showTemporaryMessage(QString message, std::chrono::milliseconds timeout)
{
    if (message.isEmpty())
        return;

    QStatusBar::showMessage(std::move(message), static_cast<int>(timeout.count()));
}

} // namespace lmrs::widgets
