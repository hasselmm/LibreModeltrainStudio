#include "packagesummary.h"

#include <lmrs/core/dccconstants.h>
#include <lmrs/core/userliterals.h>

#include "decoderinfo.h"
#include "metadata.h"
#include "package.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QXmlStreamReader>

using namespace lmrs; // FIXME
using namespace esu; // FIXME

class PackageSummary::Private : public core::PrivateObject<PackageSummary>
{
public:
    using PrivateObject::PrivateObject;

    QLineEdit *const name = new QLineEdit{q()};
    QComboBox *const country = new QComboBox{q()};
    QComboBox *const engineType = new QComboBox{q()};
    QLabel *const userImage = new QLabel{q()};
    QSpinBox *const address = new QSpinBox{q()};
    QComboBox *const decoderType = new QComboBox{q()};
    QComboBox *const projectType = new QComboBox{q()};
    QLineEdit *const projectVendor = new QLineEdit{q()};
    QLineEdit *const projectIdentifier = new QLineEdit{q()};
    QLineEdit *const projectVersion = new QLineEdit{q()};
    QPlainTextEdit *const description = new QPlainTextEdit{q()};
    QComboBox *const hifiQuality = new QComboBox{q()};

    std::shared_ptr<MetaData> metaData;
};

PackageSummary::PackageSummary(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    const auto layout = new QFormLayout{this};

    layout->addRow(tr("&Name:"), d->name);
    layout->addRow(tr("&Country:"), d->country);
    layout->addRow(tr("Engine Type:"), d->engineType);
    layout->addRow(tr("User &Image:"), d->userImage);
    layout->addRow(tr("Project Vendor:"), d->projectVendor);
    layout->addRow(tr("Project Identifier:"), d->projectIdentifier);
    layout->addRow(tr("Project Version:"), d->projectVersion);
    layout->addRow(tr("Project Type:"), d->projectType);
    layout->addRow(tr("HiFi Quality:"), d->hifiQuality);
    layout->addRow(tr("Decoder Type:"), d->decoderType);
    layout->addRow(tr("Decoder Address:"), d->address);
    layout->addRow(tr("Description:"), d->description);

    d->country->addItem(tr("Unknown"));

    for (int i = 1; i <= QLocale::LastCountry; ++i) {
        const auto country = static_cast<QLocale::Country>(i);
        const auto locales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, country);
        if (locales.isEmpty())
            continue;

        const auto countryCode = locales.first().name().split('_'_L1).at(1);
        d->country->addItem(QLocale::countryToString(country), countryCode);
    }

    d->hifiQuality->addItem(tr("Standard Quality (8 Bit)"), QVariant::fromValue(MetaData::AudioQuality::Default));
    d->hifiQuality->addItem(tr("High Quality (16 Bit)"), QVariant::fromValue(MetaData::AudioQuality::High));

    d->projectType->addItem(tr("Unknown"), QVariant::fromValue(MetaData::ProjectType::Unknown));
    d->projectType->addItem(tr("Standard"), QVariant::fromValue(MetaData::ProjectType::Standard));
    d->projectType->addItem(tr("Steam, Dual Channel"), QVariant::fromValue(MetaData::ProjectType::SteamDualChannel));

    d->engineType->addItem(tr("Unknown"), QVariant::fromValue(MetaData::EngineType::Unknown));
    d->engineType->addItem(tr("Steam Engine"), QVariant::fromValue(MetaData::EngineType::Steam));
    d->engineType->addItem(tr("Diesel Engine"), QVariant::fromValue(MetaData::EngineType::Diesel));
    d->engineType->addItem(tr("Electric Engine"), QVariant::fromValue(MetaData::EngineType::Electric));

    for (const auto &decoder: esu::DecoderInfo::all())
        d->decoderType->addItem(decoder.name, decoder.id);

    d->address->setMinimum(core::MinimumValue<core::dcc::VehicleAddress>);
    d->address->setMaximum(core::MaximumValue<core::dcc::VehicleAddress>);
}

void PackageSummary::setMetaData(std::shared_ptr<MetaData> metaData)
{
    if (std::exchange(d->metaData, metaData) == d->metaData)
        return;

    d->name->setText(metaData->name);
    d->description->setPlainText(metaData->description.toString());
    d->country->setCurrentText(metaData->country);
    d->decoderType->setCurrentText(metaData->decoderType);
    d->address->setValue(metaData->address);
    d->projectIdentifier->setText(metaData->projectIdentifier);
    d->projectVersion->setText(metaData->projectVersion);
    d->projectVendor->setText(metaData->projectVendor);
    d->userImage->setPixmap(QPixmap::fromImage(metaData->userImage));

    d->country->setCurrentIndex(qMax(0, d->country->findData(metaData->country)));
    d->engineType->setCurrentIndex(static_cast<int>(metaData->engineType));
    d->projectType->setCurrentIndex(static_cast<int>(metaData->projectType));
    d->hifiQuality->setCurrentIndex(static_cast<int>(metaData->hifiQuality));
}

std::shared_ptr<MetaData> PackageSummary::metaData() const
{
    return d->metaData;
}
