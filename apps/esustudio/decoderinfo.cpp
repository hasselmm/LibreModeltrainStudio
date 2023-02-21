#include "decoderinfo.h"

#include "lmrs/core/algorithms.h"
#include "package.h"

#include <lmrs/core/userliterals.h>

#include <QDir>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QSettings>
#include <QStandardPaths>
#include <QXmlStreamReader>


using namespace lmrs; // FIXME

namespace esu {

namespace {

Q_LOGGING_CATEGORY(lcDecoderInfo, "decoderinfo");

QString buildPath(QString root, QString path)
{
    if (root.isEmpty())
        return {};

    return QDir{root}.filePath(path);
}

QDir lokProgrammerDir()
{
    const auto windowsSettingsPath = R"(HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion)"_L1;
    const auto windowsSettings = QSettings{windowsSettingsPath, QSettings::NativeFormat};

    const auto locations = {
        QSettings{}.value("LokProgrammerDir"_L1).toString(),
        buildPath(qEnvironmentVariable("PROGRAMFILES(x86)"), "LokProgrammer5"_L1),
        buildPath(qEnvironmentVariable("PROGRAMFILESx"), "LokProgrammer5"_L1),
        buildPath(windowsSettings.value("ProgramFilesDir (x86)"_L1).toString(), "LokProgrammer5"_L1),
        buildPath(windowsSettings.value("ProgramFilesDir"_L1).toString(), "LokProgrammer5"_L1),
    };

    for (const auto &path: locations) {
        if (path.isEmpty())
            continue;

        if (const auto dir = QDir{path}; dir.exists())
            return dir;
    }

    return {};
}

QList<DecoderInfo> fallbackDecoderDatabase()
{
    return {
        {"ls5n-hv"_L1, {"LokSound 5 micro"_L1}, "LokSound V5"_L1, DecoderInfo::DecoderType::All, DecoderInfo::SoundType::V50, {}},
    };
}

/*
DecoderInfo::DecoderTypes parseDecoderType(QByteArray text)
{
    const auto meta = QMetaEnum::fromType<DecoderInfo::DecoderType>();
    const auto value = meta.keysToValue(text.constData());

    if (const auto normalized = meta.valueToKeys(value); normalized != text) {
        qCWarning(lcDecoderInfo, "Unexpected decoderInfo (\"%s\" recognized as \"%s\")",
                  text.constData(), normalized.constData());
    }

    return {value};
}
*/

DecoderInfo::DecoderTypes parseDecoderType(QByteArray text)
{
    static const auto s_mapping = QHash<QByteArray, DecoderInfo::DecoderTypes> {
        {"all", DecoderInfo::DecoderType::All},
        {"dcc", DecoderInfo::DecoderType::DCC},
        {"dcc-only", DecoderInfo::DecoderType::DCC},
        {"dcc-only.644", DecoderInfo::DecoderType::DCC}, // PluX22
        {"kato.dcc", DecoderInfo::DecoderType::Kato | DecoderInfo::DecoderType::DCC},
        {"mkl.all", DecoderInfo::DecoderType::M4 | DecoderInfo::DecoderType::Motorola},
        {"mkl.dcc", DecoderInfo::DecoderType::M4 | DecoderInfo::DecoderType::Motorola | DecoderInfo::DecoderType::DCC},
        {"m4", DecoderInfo::DecoderType::M4},
        {"std", DecoderInfo::DecoderType::Other}, // Smoke Generator
        {"eeprom", DecoderInfo::DecoderType::Other}, // Smoke Generator
    };

    if (const auto it = s_mapping.find(text); it != s_mapping.end())
        return *it;

    qCWarning(lcDecoderInfo, "Unknown decoder type: \"%s\"", text.constData());
    return {};
}

template<typename T>
std::optional<T> keyToValue(QByteArrayView text)
{
    bool found = false;
    const auto meta = QMetaEnum::fromType<T>();
    const auto value = meta.keyToValue(text.constData(), &found);

    if (Q_LIKELY(found))
        return static_cast<T>(value);

    return {};
}

DecoderInfo::SoundType parseSoundType(QByteArrayView text)
{
    if (const auto soundType = keyToValue<DecoderInfo::SoundType>(text))
        return soundType.value();

    qCWarning(lcDecoderInfo, "Unknown sound type: \"%s\"", text.constData());
    return DecoderInfo::SoundType::None;
}

QList<DecoderInfo> readDecoderDatabase()
{
    auto package = Package{};

    if (!package.read(lokProgrammerDir().filePath("DecoderInfo.dat"_L1))) {
        qCWarning(lcDecoderInfo, "Could not read decoder database from %ls",
                  qUtf16Printable(lokProgrammerDir().path()));
        return fallbackDecoderDatabase();
    }

    auto xml = QXmlStreamReader{package.data("info.xml"_L1)};

    if (!xml.readNextStartElement()
            || xml.name() != "decodertypes"_L1
            || xml.namespaceUri() != "http://www.esu.eu/2016/LOKPROGRAMMER/DecoderType"_L1
            || xml.attributes().value("version"_L1) != "0.2"_L1) {
        qCWarning(lcDecoderInfo, "Unsupported decoder database");
        return fallbackDecoderDatabase();
    }

    QList<DecoderInfo> decoderList;

    while (xml.readNextStartElement()) {
        if (xml.name() == "category"_L1) {
            while (xml.readNextStartElement()) {
                if (xml.name() == "name"_L1) {
                    qInfo() << "category:" << xml.readElementText();
                } else if (xml.name() == "decoder"_L1) {
                    auto decoder = DecoderInfo{};

                    decoder.id = xml.attributes().value("id"_L1).toString();
                    decoder.decoderTypes = parseDecoderType(xml.attributes().value("type"_L1).toLatin1());
                    decoder.group = xml.attributes().value("group"_L1).toString();

                    while (xml.readNextStartElement()) {
                        if (xml.name() == "name"_L1) {
                            decoder.name = xml.readElementText();
                        } else if (xml.name() == "sound"_L1) {
                            decoder.soundType = parseSoundType(xml.attributes().value("type"_L1).toLatin1());

                            while (xml.readNextStartElement()) {
                                if (xml.name() == "flash"_L1) {
                                    if (const auto size = core::toInt(xml.readElementText()))
                                        decoder.flashSizes.append(size.value());
                                } else {
                                    qCWarning(lcDecoderInfo, "Unsupported child of <sound> element: %ls", qUtf16Printable(xml.name().toString()));
                                    xml.skipCurrentElement();
                                }
                            }
                        } else if (xml.name() == "clear"_L1
                                   || xml.name() == "defaultversion"_L1
                                   || xml.name() == "hardware"_L1
                                   || xml.name() == "magiccode"_L1
                                   ) {
                            xml.skipCurrentElement();
                        } else {
                            qCWarning(lcDecoderInfo, "Unsupported child of <decoder> element: %ls", qUtf16Printable(xml.name().toString()));
                            xml.skipCurrentElement();
                        }
                    }

                    if (!decoder.decoderTypes)
                        qInfo() << decoder.id << decoder.name << decoder.group << decoder.decoderTypes << decoder.soundType << decoder.flashSizes;
                    decoderList.append(std::move(decoder));
                } else {
                    qCWarning(lcDecoderInfo, "Unsupported child of <category> element: %ls", qUtf16Printable(xml.name().toString()));
                    xml.skipCurrentElement();
                }
            }
        } else {
            qCWarning(lcDecoderInfo, "Unsupported child of <decodertypes> element: %ls", qUtf16Printable(xml.name().toString()));
            xml.skipCurrentElement();
        }
    }

    if (decoderList.isEmpty())
        return fallbackDecoderDatabase();

    return decoderList;
}

} // namespace

QList<DecoderInfo> DecoderInfo::all()
{
    static const auto s_allDecoders = readDecoderDatabase();
    return s_allDecoders;
}

} // namespace esu
