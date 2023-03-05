#ifndef LMRS_CORE_FILEFORMAT_H
#define LMRS_CORE_FILEFORMAT_H

#include "userliterals.h"

#include <QIODevice>
#include <QObject>
#include <QPointer>

namespace lmrs::core {

// =====================================================================================================================

struct FileFormat
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    QString name;
    QString mimeType;
    QList<WildcardLiteral> extensions;

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

};

// =====================================================================================================================

class FileFormatHandler
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    explicit FileFormatHandler(QIODevice *device);
    explicit FileFormatHandler(QString fileName);
    virtual ~FileFormatHandler();

    [[nodiscard]] auto fileName() const { return m_fileName; }
    [[nodiscard]] auto errorString() const { return m_errorString; }
    [[nodiscard]] auto succeeded() const { return m_errorString.isEmpty(); }
    [[nodiscard]] auto failed() const { return !succeeded(); }

protected:
    void reset();

    void reportError(QString errorString);
    void reportUnsupportedFileError();

    template<typename T>
    static auto fileFormats(const QList<QPair<FileFormat, T>> &registry)
    {
        auto formats = QList<FileFormat>{};
        formats.reserve(registry.size());
        std::transform(registry.begin(), registry.end(), std::back_inserter(formats),
                       [](const auto &entry) { return entry.first; });
        return formats;
    }

    bool open(QIODevice::OpenMode mode);
    bool writeData(QByteArray data);
    QByteArray readAll() const;
    bool close();

private:
    QString m_fileName;
    QString m_errorString;
    QPointer<QIODevice> m_device;
};

// =====================================================================================================================

template<typename PointerType>
struct FileFormatFactory
{
    template<class Handler>
    static FileFormatFactory make()
    {
        return {
            std::make_unique<Handler, QIODevice *>,
            std::make_unique<Handler, QString>,
        };
    }

    PointerType operator()(QIODevice *device) const noexcept { return fromDevice(device); }
    PointerType operator()(QString fileName) const noexcept { return fromFileName(std::move(fileName)); }

    std::function<PointerType(QIODevice *)> fromDevice;
    std::function<PointerType(QString)> fromFileName;

    [[nodiscard]] bool isValid() const noexcept { return fromDevice && fromFileName; }
    explicit operator bool() const noexcept { return isValid(); }
};

// =====================================================================================================================

template<typename T, typename... Args>
class FileFormatReader : public FileFormatHandler
{
public:
    using ModelType = T;
    using ModelPointer = std::unique_ptr<ModelType>;
    using Pointer = std::unique_ptr<FileFormatReader>;
    using Factory = FileFormatFactory<Pointer>;
    using Registry = QList<QPair<FileFormat, Factory>>;

    using FileFormatHandler::FileFormatHandler;

    static QList<FileFormat> fileFormats()
    {
        return FileFormatHandler::fileFormats(registry());
    }

    static void registerFileFormat(FileFormat fileFormat, Factory factory)
    {
        registry().emplaceBack(std::move(fileFormat), std::move(factory));
    }

    [[nodiscard]] static inline Factory fromFileFormat(const FileFormat &fileFormat);
    [[nodiscard]] static inline Pointer fromFileName(QString fileName);

    virtual ModelPointer read(Args...) = 0;

private:
    [[nodiscard]] static Registry &registry();
};

// ---------------------------------------------------------------------------------------------------------------------

template<typename ModelType, typename... Args>
class FallbackFileFormatReader : public FileFormatReader<ModelType, Args...>
{
public:
    using FileFormatReader<ModelType, Args...>::FileFormatReader;

    typename FallbackFileFormatReader::ModelPointer read(Args...) override
    {
        FileFormatHandler::reportUnsupportedFileError();
        return {};
    }
};

// ---------------------------------------------------------------------------------------------------------------------

template<typename ModelType, typename... Args>
inline typename FileFormatReader<ModelType, Args...>::Factory
FileFormatReader<ModelType, Args...>::fromFileFormat(const FileFormat &fileFormat)
{
    for (const auto &[format, factory]: registry())
        if (format == fileFormat)
            return factory;

    return {};
}

template<typename ModelType, typename... Args>
typename FileFormatReader<ModelType, Args...>::Pointer
FileFormatReader<ModelType, Args...>::fromFileName(QString fileName)
{
    for (const auto &[format, createReader]: registry())
        if (format.accepts(fileName))
            return createReader(std::move(fileName));

    return std::make_unique<FallbackFileFormatReader<ModelType, Args...>>(std::move(fileName));
}

// =====================================================================================================================

template<typename T>
class FileFormatWriter : public FileFormatHandler
{
public:
    using ModelType = T;
    using Pointer = std::unique_ptr<FileFormatWriter>;
    using Factory = FileFormatFactory<Pointer>;
    using Registry = QList<QPair<FileFormat, Factory>>;

    using FileFormatHandler::FileFormatHandler;

    [[nodiscard]] static QList<FileFormat> fileFormats()
    {
        return FileFormatHandler::fileFormats(registry());
    }

    static void registerFileFormat(FileFormat fileFormat, Factory factory)
    {
        registry().emplaceBack(std::move(fileFormat), std::move(factory));
    }

    [[nodiscard]] static inline Factory fromFileFormat(const FileFormat &fileFormat);
    [[nodiscard]] static inline Pointer fromFileName(QString fileName);

    virtual bool write(const ModelType *model) = 0;

private:
    [[nodiscard]] static Registry &registry();
};

// ---------------------------------------------------------------------------------------------------------------------

template<typename ModelType>
class FallbackFileFormatWriter : public FileFormatWriter<ModelType>
{
public:
    using FileFormatWriter<ModelType>::FileFormatWriter;

    bool write(const ModelType *) override
    {
        FileFormatHandler::reportUnsupportedFileError();
        return false;
    }
};

// ---------------------------------------------------------------------------------------------------------------------

template<typename ModelType>
typename FileFormatWriter<ModelType>::Factory
FileFormatWriter<ModelType>::fromFileFormat(const FileFormat &fileFormat)
{
    for (const auto &[format, factory]: registry())
        if (format == fileFormat)
            return factory;

    return {};
}

template<typename ModelType>
typename FileFormatWriter<ModelType>::Pointer
FileFormatWriter<ModelType>::fromFileName(QString fileName)
{
    for (const auto &[format, createWriter]: registry())
        if (format.accepts(fileName))
            return createWriter(std::move(fileName));

    return std::make_unique<FallbackFileFormatWriter<ModelType>>(std::move(fileName));
}

// =====================================================================================================================

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

template<typename T, typename ModelType = typename T::ModelType, typename Factory = typename T::Factory>
inline void registerFileFormat(FileFormat fileFormat)
{
    registerFileFormat<ModelType>(std::move(fileFormat), Factory::template make<T>());
}

inline size_t qHash(const lmrs::core::FileFormat &format, size_t seed = 0) noexcept
{
    return qHashMulti(seed, format.name, format.extensions);
}

} // namespace lmrs::core

// =====================================================================================================================

#endif // LMRS_CORE_FILEFORMAT_H
