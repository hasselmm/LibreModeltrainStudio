#include "fileformat.h"

#include "userliterals.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace lmrs::core {

FileFormat FileFormat::any()
{
    return {tr("All files"), {}, {"*.*"_L1}};
}

FileFormat FileFormat::csv()
{
    return {tr("Comma Separated Values"), "text/csv"_L1, {"*.csv"_L1}};
}

FileFormat FileFormat::lokProgrammer()
{
    return {tr("ESU LokProgrammer5"), {}, {"*.esux"_L1}};
}

FileFormat FileFormat::lmrsAutomationEvent()
{
    return {
        tr("Automation Event in JavaScript Object Notation"),
        "application/vnd.lmrs-automation-event+json"_L1,
        {"*.lmra"_L1, "*.json"_L1}
    };
}

FileFormat FileFormat::lmrsAutomationModel()
{
    return {
        tr("Automation Model in JavaScript Object Notation"),
        "application/vnd.lmrs-automation-model+json"_L1,
        {"*.lmra"_L1, "*.json"_L1}
    };
}

FileFormat FileFormat::plainText()
{
    return {tr("Plain text file"), "text/plain"_L1, {"*.txt"_L1}};
}

FileFormat FileFormat::tsv()
{
    return {tr("Tabulator Separated Values"), "text/tsv"_L1, {"*.tsv"_L1}};
}

FileFormat FileFormat::z21Maintenance()
{
    return {tr("Z21 Maintenance"), "text/csv"_L1, {"*.csv"_L1}};
}

FileFormat FileFormat::z21Layout()
{
    return {tr("Z21 App Layout"), {}, {"*.z21"_L1}};
}

QString FileFormat::toFilter() const
{
    return name + " ("_L1 + extensions.join(' '_L1) + ")"_L1;
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

    auto extensions = QStringList{};

    for (const auto &format: std::as_const(formatList)) {
        for (const auto &extension: format.extensions) {
            if (!extensions.contains(extension))
                extensions.append(extension);
        }
    }

    extensions.sort();

    return {std::move(name), {}, std::move(extensions)};
}

bool FileFormat::accepts(QString fileName) const
{
    // use QFileInfo to normalize the filename for matching
    fileName = QFileInfo{std::move(fileName)}.fileName();

    for (const auto &pattern: extensions) {
        if (QRegularExpression::fromWildcard(pattern).match(fileName).hasMatch())
            return true;
    }

    return false;
}

// =====================================================================================================================

FileFormatHandler::FileFormatHandler(QString fileName)
    : m_fileName{std::move(fileName)}
{}

FileFormatHandler::~FileFormatHandler()
{
    reset();
}

void FileFormatHandler::reportError(QString errorString)
{
    m_errorString = std::move(errorString);
}

void FileFormatHandler::reportUnsupportedFileError()
{
    reportError(tr("The type of this file is not recognized, or it is not supported at all."));
}

} // namespace lmrs::core
