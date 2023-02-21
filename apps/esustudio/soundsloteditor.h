#ifndef SOUNDSLOTEDITOR_H
#define SOUNDSLOTEDITOR_H

#include <QWidget>

namespace esu {
struct MetaData;
} // namespace ecu


class SoundSlotEditor : public QWidget
{
public:
    explicit SoundSlotEditor(QWidget *parent = nullptr);

    void setMetaData(std::shared_ptr<esu::MetaData> metaData);
    std::shared_ptr<esu::MetaData> metaData() const;

private:
    class Private;
    Private *const d;
};

#endif // SOUNDSLOTEDITOR_H
