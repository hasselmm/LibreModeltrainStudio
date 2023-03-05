#include "fileformat.h"

#include "logging.h"
#include "userliterals.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace lmrs::core {

FileFormat FileFormat::any()
{
    return {tr("All files"), {}, {"*.*"_wildcard}};
}

FileFormat FileFormat::csv()
{
    return {tr("Comma Separated Values"), "text/csv"_L1, {"*.csv"_wildcard}};
}

FileFormat FileFormat::lokProgrammer()
{
    return {tr("ESU LokProgrammer5"), {}, {"*.esux"_wildcard}};
}

FileFormat FileFormat::lmrsAutomationEvent()
{
    return {
        tr("Automation Event in JavaScript Object Notation"),
        "application/vnd.lmrs-automation-event+json"_L1,
        {"*.lmra"_wildcard, "*.json"_wildcard}
    };
}

FileFormat FileFormat::lmrsAutomationModel()
{
    return {
        tr("Automation Model in JavaScript Object Notation"),
        "application/vnd.lmrs-automation-model+json"_L1,
        {"*.lmra"_wildcard, "*.json"_wildcard}
    };
}

FileFormat FileFormat::plainText()
{
    return {tr("Plain text file"), "text/plain"_L1, {"*.txt"_wildcard}};
}

FileFormat FileFormat::tsv()
{
    return {tr("Tabulator Separated Values"), "text/tsv"_L1, {"*.tsv"_wildcard}};
}

FileFormat FileFormat::z21Maintenance()
{
    return {tr("Z21 Maintenance"), "text/csv"_L1, {"*.csv"_wildcard}};
}

FileFormat FileFormat::z21Layout()
{
    return {tr("Z21 App Layout"), {}, {"*.z21"_wildcard}};
}

QString FileFormat::toFilter() const
{
    auto separator = SeparatorState{};
    auto result = name + " ("_L1;

    for (const auto &wildcard: extensions) {
        result += separator.next(" "_L1);
        result += wildcard.pattern;
    }

    result += ")"_L1;
    return result;
}

QString FileFormat::openFileDialogFilter(QList<FileFormat> formatList)
{
    formatList.prepend(merged(formatList));
    formatList.append(any());

    return saveFileDialogFilter(std::move(formatList));
}

QString FileFormat::saveFileDialogFilter(QList<FileFormat> formatList)
{
    auto filterList = QStringList{};
    filterList.reserve(formatList.size());

    std::transform(formatList.begin(), formatList.end(), std::back_inserter(filterList),
                   [](const auto &format) { return format.toFilter(); });

    return filterList.join(";;"_L1);
}

FileFormat FileFormat::merged(QList<FileFormat> formatList, QString name)
{
    if (name.isEmpty())
        return merged(std::move(formatList), tr("Supported files"));

    auto extensions = QList<WildcardLiteral>{};

    for (const auto &format: std::as_const(formatList)) {
        for (const auto &extension: format.extensions) {
            if (!extensions.contains(extension))
                extensions.append(extension);
        }
    }

    std::sort(extensions.begin(), extensions.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.pattern < rhs.pattern;
    });

    return {std::move(name), {}, std::move(extensions)};
}

bool FileFormat::accepts(QString fileName) const
{
    // use QFileInfo to normalize the filename for matching
    fileName = QFileInfo{std::move(fileName)}.fileName();

    for (const auto &pattern: extensions) {
        if (pattern.match(fileName).hasMatch())
            return true;
    }

    return false;
}

// =====================================================================================================================

FileFormatHandler::FileFormatHandler(QIODevice *device)
    : m_device{device}
{}

FileFormatHandler::FileFormatHandler(QString fileName)
    : m_fileName{std::move(fileName)}
{}

FileFormatHandler::~FileFormatHandler()
{
    reset();
}

void FileFormatHandler::reset()
{
    if (m_device && m_device->isOpen())
        m_device->close();
    if (!m_fileName.isEmpty())
        delete m_device;

    m_errorString.clear();
}

void FileFormatHandler::reportError(QString errorString)
{
    m_errorString = std::move(errorString);
}

void FileFormatHandler::reportUnsupportedFileError()
{
    reportError(tr("The type of this file is not recognized, or it is not supported at all."));
}

bool FileFormatHandler::open(QIODeviceBase::OpenMode mode)
{
    if (m_device.isNull())
        m_device = new QFile{m_fileName};

    if (!m_device->open(mode)) {
        reportError(m_device->errorString());
        return false;
    }

    return true;
}

bool FileFormatHandler::writeData(QByteArray data)
{
    if (LMRS_FAILED(logger(this), !m_device.isNull())
            || LMRS_FAILED(logger(this), m_device->isOpen()))
        return false;

    const auto bytesToWrite = data.length();

    if (m_device->write(std::move(data)) != bytesToWrite) {
        reportError(m_device->errorString());
        return false;
    }

    return true;
}

QByteArray FileFormatHandler::readAll() const
{
    if (LMRS_FAILED(logger(this), !m_device.isNull())
            || LMRS_FAILED(logger(this), m_device->isOpen()))
        return {};

    return m_device->readAll();
}

bool FileFormatHandler::close()
{
    if (LMRS_FAILED(logger(this), !m_device.isNull())
            || LMRS_FAILED(logger(this), m_device->isOpen()))
        return false;

    m_device->close();

    if (!m_fileName.isEmpty())
        delete m_device;

    return true;
}

} // namespace lmrs::core
