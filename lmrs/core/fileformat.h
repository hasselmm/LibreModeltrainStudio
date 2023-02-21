#ifndef LMRS_CORE_FILEFORMAT_H
#define LMRS_CORE_FILEFORMAT_H

#include <QObject>

namespace lmrs::core {

// =====================================================================================================================

struct FileFormat
{
    QString name;
    QString mimeType;
    QStringList extensions;

    [[nodiscard]] QString toFilter() const;

    [[nodiscard]] static FileFormat any();
    [[nodiscard]] static FileFormat csv();
    [[nodiscard]] static FileFormat lokProgrammer();
    [[nodiscard]] static FileFormat lmrsAutomationEvent();
    [[nodiscard]] static FileFormat lmrsAutomationModel();
    [[nodiscard]] static FileFormat plainText();
    [[nodiscard]] static FileFormat tsv();
    [[nodiscard]] static FileFormat z21Layout();
    [[nodiscard]] static FileFormat z21Maintenance();
    [[nodiscard]] static FileFormat z21Vehicle();

    [[nodiscard]] static QString openFileDialogFilter(QList<FileFormat> formatList);
    [[nodiscard]] static QString saveFileDialogFilter(QList<FileFormat> formatList);

    [[nodiscard]] static FileFormat merged(QList<FileFormat> formatList, QString name = {});

    [[nodiscard]] auto fields() const noexcept { return std::tie(name, extensions); }
    [[nodiscard]] auto operator==(const FileFormat &rhs) const noexcept { return fields() == rhs.fields(); }
    [[nodiscard]] auto operator!=(const FileFormat &rhs) const noexcept { return fields() != rhs.fields(); }

    [[nodiscard]] bool accepts(QString fileName) const;

    Q_GADGET
    QT_TR_FUNCTIONS
};

// =====================================================================================================================

namespace internal {

class FileFormatTranslations
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    static QString unsupportedFileType();
};

template<typename T>
auto fileFormats(const QList<QPair<FileFormat, T>> &registry)
{
    auto formats = QList<FileFormat>{};
    formats.reserve(registry.size());
    std::transform(registry.begin(), registry.end(), std::back_inserter(formats),
                   [](const auto &entry) { return entry.first; });
    return formats;
}

} // namespace interal

// =====================================================================================================================

template<typename T, typename... Args>
class FileFormatReader
{
public:
    using ModelType = T;
    using ModelPointer = std::unique_ptr<ModelType>;

    using Pointer = std::unique_ptr<FileFormatReader>;
    using Factory = std::function<Pointer(QString)>;
    using Registry = QList<QPair<FileFormat, Factory>>;

    explicit FileFormatReader(QString fileName) : m_fileName{std::move(fileName)} {}
    virtual ~FileFormatReader() { reset(); }

    static QList<FileFormat> fileFormats() { return internal::fileFormats(registry()); }
    static Pointer fromFile(QString fileName);

    virtual ModelPointer read(Args...) = 0;

    [[nodiscard]] auto fileName() const { return m_fileName; }
    [[nodiscard]] auto errorString() const { return m_errorString; }

    static void registerFileFormat(FileFormat fileFormat, Factory factory)
    {
        registry().emplaceBack(std::move(fileFormat), std::move(factory));
    }

protected:
    void reportError(QString errorString) { m_errorString = std::move(errorString); }
    virtual void reset() { m_errorString.clear(); }

private:
    [[nodiscard]] static Registry &registry();

    QString m_fileName;
    QString m_errorString;
};

// ---------------------------------------------------------------------------------------------------------------------

template<typename ModelType, typename... Args>
class FallbackFileFormatReader : public FileFormatReader<ModelType, Args...>
{
public:
    using FileFormatReader<ModelType, Args...>::FileFormatReader;

    typename FallbackFileFormatReader::ModelPointer read(Args...) override
    {
        FallbackFileFormatReader::reportError(internal::FileFormatTranslations::unsupportedFileType());
        return {};
    }
};

// =====================================================================================================================

template<typename T>
class FileFormatWriter
{
public:
    using ModelType = T;

    using Pointer = std::unique_ptr<FileFormatWriter>;
    using Factory = std::function<Pointer(QString)>;
    using Registry = QList<QPair<FileFormat, Factory>>;

    explicit FileFormatWriter(QString fileName) : m_fileName{std::move(fileName)} {}
    virtual ~FileFormatWriter() { reset(); }

    [[nodiscard]] static QList<FileFormat> fileFormats() { return internal::fileFormats(registry()); }
    [[nodiscard]] static inline Pointer fromFile(QString fileName);

    virtual bool write(const ModelType *model) = 0;

    [[nodiscard]] auto fileName() const noexcept { return m_fileName; }
    [[nodiscard]] auto errorString() const noexcept { return m_errorString; }

    static void registerFileFormat(FileFormat fileFormat, Factory factory)
    {
        registry().emplaceBack(std::move(fileFormat), std::move(factory));
    }

protected:
    void reportError(QString errorString) { m_errorString = std::move(errorString); }
    virtual void reset() { m_errorString.clear(); }

private:
    [[nodiscard]] static Registry &registry();

    QString m_fileName;
    QString m_errorString;
};

// ---------------------------------------------------------------------------------------------------------------------

template<typename ModelType>
class FallbackFileFormatWriter : public FileFormatWriter<ModelType>
{
public:
    using FileFormatWriter<ModelType>::FileFormatWriter;

    bool write(const ModelType *) override
    {
        FallbackFileFormatWriter::reportError(internal::FileFormatTranslations::unsupportedFileType());
        return false;
    }
};

// =====================================================================================================================

template<typename ModelType, typename... Args>
typename FileFormatReader<ModelType, Args...>::Pointer
FileFormatReader<ModelType, Args...>::fromFile(QString fileName)
{
    for (const auto &[format, createReader]: registry())
        if (format.accepts(fileName))
            return createReader(std::move(fileName));

    return std::make_unique<FallbackFileFormatReader<ModelType, Args...>>(std::move(fileName));
}

template<typename ModelType>
typename FileFormatWriter<ModelType>::Pointer
FileFormatWriter<ModelType>::fromFile(QString fileName)
{
    for (const auto &[format, createWriter]: registry())
        if (format.accepts(fileName))
            return createWriter(std::move(fileName));

    return std::make_unique<FallbackFileFormatWriter<ModelType>>(std::move(fileName));
}

// ---------------------------------------------------------------------------------------------------------------------

template<typename ModelType>
inline void registerFileFormat(FileFormat fileFormat, typename FileFormatReader<ModelType>::Factory factory)
{
    FileFormatReader<ModelType>::registerFileFormat(std::move(fileFormat), std::move(factory));
}

template<typename ModelType>
inline void registerFileFormat(FileFormat fileFormat, typename FileFormatWriter<ModelType>::Factory factory)
{
    FileFormatWriter<ModelType>::registerFileFormat(std::move(fileFormat), std::move(factory));
}

// ---------------------------------------------------------------------------------------------------------------------

template<typename T, typename ModelType = typename T::ModelType>
inline void registerFileFormat(FileFormat fileFormat)
{
    registerFileFormat<ModelType>(std::move(fileFormat), std::make_unique<T, QString>);
}

} // namespace lmrs::core

// =====================================================================================================================

template<>
struct std::hash<lmrs::core::FileFormat>
{
    size_t operator()(const lmrs::core::FileFormat &format, size_t seed)
    {
        return qHashMulti(seed, format.name, format.extensions);
    }
};

#endif // LMRS_CORE_FILEFORMAT_H
