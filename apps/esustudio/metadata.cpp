#include "metadata.h"

#include "package.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/xmlstreamparser.h>
#include <lmrs/core/userliterals.h>

#include <QLocale>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QStack>
#include <QXmlStreamReader>

using namespace lmrs; // FIXME

namespace esu {

namespace {

constexpr auto lcMetaData = qOverload<>(&core::logger<MetaData>);

auto parseEngineType(QString text)
{
    if (text == "steam"_L1)
        return MetaData::EngineType::Steam;
    else if (text == "diesel"_L1)
        return MetaData::EngineType::Diesel;
    else if (text == "electric"_L1)
        return MetaData::EngineType::Electric;

    qCWarning(lcMetaData, "Unsupported engine type: %ls", core::printable(text));
    return MetaData::EngineType::Unknown;
}

auto parseHiAudioQuality(QString text)
{
    bool isNumber = false;
    const auto value = text.toInt(&isNumber);

    if (Q_UNLIKELY(!isNumber) ||
            Q_UNLIKELY(!QMetaEnum::fromType<MetaData::AudioQuality>().valueToKey(value))) {
        qCWarning(lcMetaData, "Unsupported HiFi quality: %ls", core::printable(text));
        return MetaData::AudioQuality::Default;
    }

    return static_cast<MetaData::AudioQuality>(value);
}

auto parseProjectType(QStringView text)
{
    if (text == "standard"_L1)
        return MetaData::ProjectType::Standard;
    else if (text == "steam2ch"_L1)
        return MetaData::ProjectType::SteamDualChannel;

    qCWarning(lcMetaData, "Unsupported project type: %ls", core::printable(text));
    return MetaData::ProjectType::Unknown;
}

QDebug &operator<<(QDebug &debug, QXmlStreamAttributes attributes)
{
    QMap<QString, QString> map;

    for (const auto &attr: attributes)
        map.insert(attr.qualifiedName().toString(), attr.value().toString());

    debug << map;
    return debug;
}

} // namespace

void LocalizedString::insert(QString lang, QString value)
{
    m_values.insert(std::move(lang), std::move(value));
}

QString LocalizedString::toString() const
{
    const auto currentLanguage = QLocale{}.bcp47Name().split('-'_L1).first();
    if (const auto it = m_values.find(currentLanguage); it != m_values.end())
        return it.value();

    return m_values.value("en"_L1);
}

std::shared_ptr<MetaData> MetaData::fromPackage(std::shared_ptr<const Package> package)
{
    auto metaData = std::make_shared<MetaData>();

    if (!metaData->readManifest(package)
            || !metaData->readSoundSlots(package)
            || !metaData->readSpecification(package))
        return {};

    metaData->userImage.loadFromData(package->data("#picture"_L1));

    return metaData;
}

bool MetaData::readManifest(std::shared_ptr<const Package> package)
{
    auto xml = QXmlStreamReader{package->data("#metadata"_L1)};

    if (!xml.readNextStartElement()
            || xml.name() != "meta"_L1
            || xml.namespaceUri() != "http://www.esu.eu/2010/LOKPROGRAMMER/Metadata"_L1) {
        qCWarning(lcMetaData, "Invalid document type for metadata");
        return false;
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == "name"_L1) {
            name = xml.readElementText();
        } else if (xml.name() == "country"_L1) {
            country = xml.readElementText();
        } else if (xml.name() == "address"_L1) {
            address = xml.readElementText().toInt();
        } else if (xml.name() == "project"_L1) {
            projectVersion = xml.attributes().value("version"_L1).toString();
            projectIdentifier = xml.readElementText();
        } else if (xml.name() == "manufacturer"_L1) {
            projectVendor = xml.readElementText();
        } else if (xml.name() == "type"_L1) {
            engineType = parseEngineType(xml.readElementText());
        } else if (xml.name() == "decoder"_L1) {
            decoderType = xml.readElementText();
        } else if (xml.name() == "hifiquality"_L1) {
            hifiQuality = parseHiAudioQuality(xml.readElementText());
        } else if (xml.name() == "description"_L1) {
            const auto language = core::coalesce(xml.attributes().value("xml:lang"_L1).toString(), "en"_L1);
            description.insert(language, xml.readElementText());
        } else if (xml.name() == "function"_L1) {
            auto function = Function{};
            function.id = xml.attributes().value("id"_L1).toInt();
            function.config = (xml.attributes().value("user"_L1).toString() == "true"_L1) ? Function::UserIndexFirst : 0;

            while (xml.readNextStartElement()) {
                if (xml.name() == "description"_L1) {
                    const auto language = core::coalesce(xml.attributes().value("xml:lang"_L1).toString(), "en"_L1);
                    function.description.insert(language, xml.readElementText());
                } else if (xml.name() == "slot"_L1) {
                    function.slot.volume = xml.attributes().value("volume"_L1).toInt();
                    function.slot.id = xml.readElementText().toInt();
                } else {
                    qCWarning(lcMetaData, "Unsupported child of <function> element: %ls", core::printable(xml.name().toString()));
                    xml.skipCurrentElement();
                }
            }

            functions.append(std::move(function));
        } else if (xml.name() == "output"_L1) {
            auto output = Output{};
            output.config = xml.attributes().value("config"_L1).toInt();
            output.number = xml.attributes().value("number"_L1).toInt();

            while (xml.readNextStartElement()) {
                if (xml.name() == "description"_L1) {
                    const auto language = core::coalesce(xml.attributes().value("xml:lang"_L1).toString(), "en"_L1);
                    output.description.insert(language, xml.readElementText());
                } else {
                    qCWarning(lcMetaData, "Unsupported child of <output> element: %ls", core::printable(xml.name().toString()));
                    xml.skipCurrentElement();
                }
            }

            outputs.append(std::move(output));
        } else if (xml.name() == "soundslot"_L1) {
            auto slot = SoundSlot{};

            slot.id = xml.attributes().value("id"_L1).toInt() + 1;

            while (xml.readNextStartElement()) {
                if (xml.name() == "description"_L1) {
                    const auto language = core::coalesce(xml.attributes().value("xml:lang"_L1).toString(), "en"_L1);
                    slot.description.insert(language, xml.readElementText());
                } else {
                    qCWarning(lcMetaData, "Unsupported child of <soundslot> element: %ls", core::printable(xml.name().toString()));
                    xml.skipCurrentElement();
                }
            }

            soundSlots.insert(slot.id, std::move(slot));
        } else if (xml.name() == "soundcv"_L1) {
            auto variable = SoundVariable{};
            variable.number = xml.attributes().value("number"_L1).toInt();
            variable.value = xml.attributes().value("value"_L1).toInt();

            while (xml.readNextStartElement()) {
                if (xml.name() == "description"_L1) {
                    const auto language = core::coalesce(xml.attributes().value("xml:lang"_L1).toString(), "en"_L1);
                    variable.description.insert(language, xml.readElementText());
                } else {
                    qCWarning(lcMetaData, "Unsupported child of <soundcv> element: %ls", core::printable(xml.name().toString()));
                    xml.skipCurrentElement();
                }
            }

            soundVariables.append(std::move(variable));
        } else if (xml.name() == "lokprogrammer"_L1) {
            xml.skipCurrentElement();
        } else {
            qCWarning(lcMetaData, "Unsupported child of <meta> element: %ls", core::printable(xml.name().toString()));
            xml.skipCurrentElement();
        }

//        "lokprogrammer"
//        ""
    }

    return true;
}

bool MetaData::readSoundSlots(std::shared_ptr<const Package> package)
{
    auto xml = QXmlStreamReader{package->data("ls5_project.xml"_L1)};

    if (!xml.readNextStartElement()
            || xml.name() != "sound"_L1
            || xml.namespaceUri() != "http://www.esu.eu/2009/DECODER/sound"_L1) {
        qCWarning(lcMetaData, "Invalid document type for sound project");
        return false;
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == "project"_L1) {
            projectType = parseProjectType(xml.attributes().value("type"_L1));

            while (xml.readNextStartElement()) {
                if (xml.name() == "slot"_L1) {
                    const auto id = xml.attributes().value("id"_L1).toInt();
                    soundSlots[id].path = xml.attributes().value("link"_L1).toString();
                    xml.skipCurrentElement();
                } else {
                    qCWarning(lcMetaData, "Unsupported child of <project> element: %ls",
                              core::printable(xml.name().toString()));
                    xml.skipCurrentElement();
                }
            }
        } else {
            qCWarning(lcMetaData, "Unsupported child of <sound> element: %ls",
                      core::printable(xml.name().toString()));
            xml.skipCurrentElement();
        }
    }

    return true;
}

bool MetaData::readSpecification(std::shared_ptr<const Package> package)
{
    class SpecificationParser : public core::xml::StreamParser<SpecificationParser>
    {
    public:
        using StreamParser::StreamParser;
        friend class StreamParser;

        auto items() const { return m_items; }

    protected:
        bool readDocumentRoot()
        {
            if (xml().name() == "decoder"_L1
                    && xml().namespaceUri() == "http://www.esu.eu/2008/DECODER/specs"_L1)
                return enterState(&SpecificationParser::readDecoderElement);

            return false;
        }

        bool readDecoderElement()
        {
            if (xml().name() == "hardware"_L1)
                return enterState(&SpecificationParser::readHardwareElement);

            return false;
        }

        bool readHardwareElement()
        {
            if (xml().name() == "firmware"_L1)
                return enterState(&SpecificationParser::readFirmwareElement);

            return false;
        }

        bool readFirmwareElement()
        {
            if (xml().name() == "code"_L1)
                return skipUnimplementedElement();

            if (xml().name() == "configuration"_L1) {
                qCInfo(lcMetaData) << "configuration:" << xml().attributes();
                return enterState(&SpecificationParser::readConfigurationElement);
            }

            return false;
        }

        bool readConfigurationElement()
        {
            if (xml().name() == "variable"_L1)
                return enterState(&SpecificationParser::readVariableElement);

            return false;
        }

        bool readVariableElement()
        {
            if (xml().name() == "item"_L1) {
                m_currentItemName = xml().attributes().value("name"_L1).toString();
                return enterState(&SpecificationParser::readItemElement);
            }

            if (xml().name() == "exec"_L1)
                return skipUnimplementedElement();

            return false;
        }

        bool readItemElement()
        {
            if (xml().name() == "value"_L1) {
                m_items.insert(m_currentItemName, xml().readElementText().toInt());
                return true;
            }

            if (xml().name() == "case"_L1)
                return skipUnimplementedElement();

            return false;
        }

    private:
        QHash<QString, int> m_items;
        QString m_currentItemName;
    };

    auto specification = SpecificationParser{lcMetaData, package->data("specification.xml"_L1)};

    if (!specification.parse())
        return false;

    for (const auto items = specification.items(); auto &fn: functions) {
        const auto high = items.constFind(u"info.nmra.key.%1.icon.0"_qs.arg(fn.id));
        const auto low = items.constFind(u"info.nmra.key.%1.icon.1"_qs.arg(fn.id));

        if (high != items.constEnd() && low != items.constEnd())
            fn.config = (high.value() << 8) | low.value();
    }

    return true;
}

// namespace


} // namespace esu
