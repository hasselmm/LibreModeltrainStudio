#include "package.h"

#include "metadata.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/userliterals.h>

#include <QFile>
#include <QLoggingCategory>

#include <QtEndian>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include <zlib.h>

template<>
struct std::default_delete<EVP_CIPHER_CTX>
{
    void operator()(EVP_CIPHER_CTX *ctx)
    {
        EVP_CIPHER_CTX_free(ctx);
    }
};

using namespace lmrs; // FIXME

namespace esu {

namespace {

Q_LOGGING_CATEGORY(lcPackage, "package");
Q_LOGGING_CATEGORY(lcOpenSSL, "openssl");

constexpr auto s_factId_esuPackage5a = ":/taschenorakel.de/lmrs/esustudio/assets/esu-package-5a.png"_L1;

QByteArray readFact(QString fileName)
{
    auto fact = QImage{fileName};

    if (fact.isNull()) {
        qCWarning(core::logger<Package>(), "Could not load: %ls", qUtf16Printable(fileName));
        return {};
    }

    if (fact.size() != QSize{256, 256}) {
        qCWarning(core::logger<Package>(), "Invalid: %ls", qUtf16Printable(fileName));
        return {};
    }

    fact = fact.convertToFormat(QImage::Format_RGB16).scaled(4, 4);
    return {reinterpret_cast<const char *>(fact.constBits()), fact.sizeInBytes()};
}

template <typename T>
T readInt(QIODevice *device)
{
    const auto data = device->read(sizeof(T));
    return qFromLittleEndian<T>(data.constData());
}

QByteArray gUncompress(QByteArray data)
{
    if (data.size() <= 4) {
        qWarning("gUncompress: Input data is truncated");
        return {};
    }

    z_stream strm;
    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];
    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = static_cast<unsigned>(data.size());
    strm.next_in = reinterpret_cast<Byte *>(data.data());

    // gzip decoding
    if (auto rc = inflateInit2(&strm, 15 + 32); rc != Z_OK)
        return {};

    QByteArray result;

    //run inflate()
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = (Bytef*)(out);
        int ret;
        ret = inflate(&strm, Z_NO_FLUSH);
        Q_ASSERT(ret != Z_STREAM_ERROR);  //state not clobbered
        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;     //and fall through
            [[fallthrough]];
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            (void)inflateEnd(&strm);
            return QByteArray();
        }
        result.append(out, CHUNK_SIZE - strm.avail_out);
    } while (strm.avail_out == 0);

    //clean up and return
    inflateEnd(&strm);
    return result;
}

template<int line>
int forwardOpenSSLError(const char *msg, size_t, void *)
{
    qCWarning(lcOpenSSL, "%s", msg);
    return 1;
}

} // namespace

Chunk::Chunk(Type type, QByteArray data)
    : m_type{type}
    , m_data{std::move(data)}
{}

Chunk Chunk::read(QIODevice *device)
{
    const auto type = static_cast<Type>(readInt<quint16>(device));
    auto len = readInt<qint32>(device);

    if (type == Type::EncryptedData || type == Type::EncryptedDataCompressed)
        len -= 4;

    auto bytes = QByteArray{len, Qt::Uninitialized};
    device->read(bytes.data(), len);
    return {type, std::move(bytes)};
}

VersionChunk::VersionChunk()
    : Chunk{Type::Package, {2, Qt::Uninitialized}}
{}

/*
VersionChunk::VersionChunk(int major, int minor)
    : VersionChunk{}
{
    m_data[1] = major;
    m_data[0] = minor;
}
*/

int VersionChunk::major() const
{
    return m_data[1];
}

int VersionChunk::minor() const
{
    return m_data[0];
}

QString VersionChunk::toString() const
{
    return QString::number(major()) + '.'_L1 + QString::number(minor());
}

bool Package::read(QString fileName)
{
    auto file = QFile{fileName};

    if (!file.open(QFile::ReadOnly)) {
        m_errorString = file.errorString();
        return false;
    }

    return read(&file);
}

bool Package::read(QIODevice *device)
{
    if (device->read(4) != "ESU ") {
        m_errorString = tr("File is corrupt");
        return false;
    }

    const auto version = VersionChunk{Chunk::read(device)};

    if (version.size() == 0
            || version.type() != Chunk::Type::Package) {
        m_errorString = tr("File is corrupt");
        return false;
    }

    if (version.major() > 0 ||
            (version.major() == 0 && version.minor() > 1)) {
        m_errorString = tr("Unsupported version: %1").arg(version.toString());
        return false;
    }

    qCDebug(lcPackage, "package format %ls", qUtf16Printable(version.toString()));

    QMultiMap<quint64, QString> fileNames;
    QHash<quint64, QByteArray> fileData;

    while (!device->atEnd()) {
//        const auto pos = device->pos();
        auto chunk = Chunk::read(device);

//        qInfo() << "offset:" << pos
//                << "size:" << chunk.data().size()
//                << chunk.type();

        if (chunk.type() == Chunk::Type::Metadata) {
            m_entries.append({"#metadata"_L1, chunk.data()});
        }

        if (chunk.type() == Chunk::Type::Bitmap) {
            const auto data = chunk.data();
//            const auto type = static_cast<BinaryChunk::BinaryType>(data[0]);
            auto contents = data.mid(1);

            auto file = QFile{"test.bmp"_L1};
            file.open(QFile::WriteOnly);
            file.write(contents);

            m_entries.append({"#picture"_L1, std::move(contents)});
        }

        if (chunk.type() == Chunk::Type::FileInfo) {
            auto fileName = QString::fromUtf8(chunk.data().left(chunk.size() - 16));
            const auto index = qFromLittleEndian<quint64>(chunk.data().constData() + chunk.size() - 16);
//            const auto v2 = qFromLittleEndian<quint64>(chunk.data().constData() + chunk.size() - 8);
//            qInfo() << fileName << "index:" << index << "v2:" << v2; // FIXME: after that two 64 numbers
            fileNames.insert(index, std::move(fileName));
        }

        if (chunk.type() == Chunk::Type::EncryptedData
                || chunk.type() == Chunk::Type::EncryptedDataCompressed) {
            const auto data = chunk.data();
            const auto index = qFromLittleEndian<quint64>(chunk.data().constData());
//            const auto v2 = qFromLittleEndian<quint64>(chunk.data().constData() + 8);
            const auto iv = data.mid(16, 16);
            const auto encrypted = data.mid(32) + QByteArray{16, Qt::Uninitialized};

//            qInfo() << "index:" << index
//                    << "v2:" << v2
//                    << "iv:" << iv.toHex()
//                    << "size:" << encrypted.size();

            const auto key = readFact(s_factId_esuPackage5a);
            const auto ctx = std::unique_ptr<EVP_CIPHER_CTX>{EVP_CIPHER_CTX_new()};
            auto decrypted = QByteArray{encrypted.size(), Qt::Uninitialized}; // FIXME: where are the 16 bytes comming from?

            if (!ctx) {
                qCWarning(lcPackage, "Could not create EVP context");
                ERR_print_errors_cb(&forwardOpenSSLError<__LINE__>, this);
                return false;
            }

            if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr,
                                   reinterpret_cast<const uchar *>(key.constData()),
                                   reinterpret_cast<const uchar *>(iv.constData())) != 1) {
                qCWarning(lcPackage, "Could not set encyption key");
                ERR_print_errors_cb(&forwardOpenSSLError<__LINE__>, this);
                return false;
            }

            auto decryptedTail = reinterpret_cast<uchar *>(decrypted.data());
            auto bytesDecrypted = 0;

            if (EVP_DecryptUpdate(ctx.get(), decryptedTail, &bytesDecrypted,
                                  reinterpret_cast<const uchar *>(encrypted.constData()),
                                  static_cast<int>(encrypted.size() - 16)) != 1) {
                qCWarning(lcPackage, "Could not decrypt data");
                ERR_print_errors_cb(&forwardOpenSSLError<__LINE__>, this);
                return false;
            }

            decryptedTail += bytesDecrypted;

            if (EVP_DecryptFinal_ex(ctx.get(), decryptedTail, &bytesDecrypted) != 1) {
                qCWarning(lcPackage, "Could not finalize decryption");
                ERR_print_errors_cb(&forwardOpenSSLError<__LINE__>, this);
                return false;
            }

            if (chunk.type() == Chunk::Type::EncryptedDataCompressed)
                decrypted = gUncompress(decrypted);

            fileData.insert(index, std::move(decrypted));
        }
    }

    for (auto file = fileNames.begin(); file != fileNames.end(); ++file) {
        if (const auto data = fileData.find(file.key()); data != fileData.end()) {
            m_entries.append({file.value(), data.value()});

            if (const auto next = std::next(file);
                    next == fileNames.end() || next.key() != file.key())
                fileData.erase(data);
        } else {
            qCWarning(lcPackage) << "No data found for" << file.value() << file.key();
        }
    }

    if (!fileData.isEmpty())
        qCWarning(lcPackage) << "No filenames for" << fileData.keys();

    return true;
}

QString Package::errorString() const
{
    return m_errorString;
}

bool Package::hasIndex(int index) const
{
    return (index >= 0 && index < m_entries.count());
}

int Package::indexOf(QString fileName) const
{
    for (auto i = 0; i < fileCount(); ++i) {
        if (this->fileName(i) == fileName)
            return i;
    }

    return -1;
}

QString Package::fileName(int index) const
{
    if (hasIndex(index))
        return m_entries.at(index).fileName;

    return {};
}

QByteArray Package::data(QString fileName) const
{
    return data(indexOf(fileName));
}

QByteArray Package::data(int index) const
{
    if (hasIndex(index))
        return m_entries.at(index).data;

    return {};
}

qint64 Package::size(int index) const
{
    if (hasIndex(index))
        return m_entries.at(index).data.size();

    return {};
}

qsizetype Package::fileCount() const
{
    return m_entries.count();
}

} // namespace esu

