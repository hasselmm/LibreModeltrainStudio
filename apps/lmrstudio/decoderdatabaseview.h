#ifndef LMRS_STUDIO_DECODERDATABASEVIEW_H
#define LMRS_STUDIO_DECODERDATABASEVIEW_H

#include <QWidget>

namespace lmrs::studio {

class DecoderDatabaseView : public QWidget
{
    Q_OBJECT

public:
    explicit DecoderDatabaseView(QWidget *parent = nullptr);
    ~DecoderDatabaseView() override;

private:
    class Private;
    Private *const d;
};

} // namespace lmrs::studio

#endif // LMRS_STUDIO_DECODERDATABASEVIEW_H
