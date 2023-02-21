#ifndef ESU_DECODERINFO_H
#define ESU_DECODERINFO_H

#include <QObject>

namespace esu {

struct DecoderInfo
{
    enum class DecoderType {
        DCC = (1 << 0),
        Motorola = (1 << 1),
        Selectrix = (1 << 2),
        M4 = (1 << 3),
        Kato = (1 << 4),
        Other = (1 << 30),

        All = DCC|Motorola|Selectrix|M4,
        None = 0,
    };

    Q_FLAG(DecoderType)
    Q_DECLARE_FLAGS(DecoderTypes, DecoderType)

    enum class SoundType {
        None = 0,
        V30 = 30,
        V35 = 35,
        V40 = 40,
        V50 = 50,
    };

    Q_ENUM(SoundType);

    QString id;
    QString name;
    QString group;

    DecoderTypes decoderTypes;
    SoundType soundType = SoundType::None;
    QList<int> flashSizes;

    static QList<DecoderInfo> all();

    Q_GADGET
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DecoderInfo::DecoderTypes)

} // namespace esu

#endif // ESU_DECODERINFO_H
