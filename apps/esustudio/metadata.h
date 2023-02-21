#ifndef METADATA_H
#define METADATA_H

#include <lmrs/core/userliterals.h>

#include <QObject>
#include <QMap>
#include <QImage>

#include <memory>

namespace esu {

class Package;

class LocalizedString
{
public:
    LocalizedString() = default;
    LocalizedString(QString fallback)
        : m_values{{"en"_L1, std::move(fallback)}}
    {}

    void insert(QString lang, QString value);
    QString toString() const;

private:
    QMap<QString, QString> m_values;
};

struct Function
{
    enum class Category {
        Light,
        Generic,
        Sound,
        Logical,
    };

    Q_ENUM(Category)

    int id = 0;
    int config = 0;

    LocalizedString description;

    struct {
        int id = 0;
        int volume = 0;
    } slot;

    constexpr auto index() const { return config & 0x0fff; }
    constexpr auto category() const { return static_cast<Category>((config >> 13) & 3); }
    constexpr auto isUser() const { return index() >= UserIndexFirst; }
    constexpr auto isMomentary() const { return !!(config & 0x8000); }
    constexpr auto isInverted() const { return !!(config & 0x1000); }

    constexpr static auto UserIndexFirst = 0xf80;

    Q_GADGET
};

struct Output
{
    int config = 0;
    int number = 0;
    LocalizedString description;
};

struct SoundSlot
{
    int id = 0;
    QString path;
    LocalizedString description;
};

struct SoundVariable
{
    int number = 0;
    int value = 0;
    LocalizedString description;
};

struct MetaData
{
    enum class EngineType {
        Unknown,
        Steam,
        Diesel,
        Electric,
    };

    Q_ENUM(EngineType)

    enum class AudioQuality {
        Default,
        High,
    };

    Q_ENUM(AudioQuality)

    enum class ProjectType {
        Unknown,
        Standard,
        SteamDualChannel,
    };

    Q_ENUM(ProjectType)

    QString name;
    LocalizedString description;
    QString country;
    int address = 0;
    QString decoderType;
    QString projectVersion;
    QString projectIdentifier;
    QString projectVendor;
    QImage userImage;

    EngineType engineType = EngineType::Unknown;
    ProjectType projectType = ProjectType::Unknown;
    AudioQuality hifiQuality = AudioQuality::Default;

    QList<Function> functions;
    QList<Output> outputs;
    QMap<int, SoundSlot> soundSlots;
    QList<SoundVariable> soundVariables;

    static std::shared_ptr<MetaData> fromPackage(std::shared_ptr<const Package> package);
    static QString localizedString(LocalizedString translations);

private:
    bool readManifest(std::shared_ptr<const Package> package);
    bool readSoundSlots(std::shared_ptr<const Package> package);
    bool readSpecification(std::shared_ptr<const Package> package);

    Q_GADGET
};

} // namespace esu

#endif // METADATA_H
