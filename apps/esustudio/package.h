#ifndef ESU_CORE_PACKAGE_H
#define ESU_CORE_PACKAGE_H

#include <QImage>
#include <QMap>
#include <QObject>

class QIODevice;

namespace esu {

class Chunk
{
    Q_GADGET

public:
    enum class Type {
        Package = 0,
        Metadata = 16,
        Bitmap,
        FileInfo = 32,
        EncryptedData,
        EncryptedDataCompressed,
    };

    Q_ENUM(Type)

    Chunk(Type type, QByteArray data);

    auto type() const { return m_type; }
    auto data() const { return m_data; }
    auto size() const { return m_data.size(); }

    static Chunk read(QIODevice *device);

protected:
    Type m_type;
    QByteArray m_data;
};

class VersionChunk : public Chunk
{
public:
    explicit VersionChunk();
//    explicit VersionChunk(int major, int minor);

    VersionChunk(Chunk chunk)
        : Chunk{chunk.type(), chunk.data()}
    {}

    int major() const;
    int minor() const;

    QString toString() const;
};

class BinaryChunk : public Chunk
{
    Q_GADGET

public:
    enum BinaryType {
        Picture = 0,
    };

    Q_ENUM(BinaryType)
};

class Package : public QObject
{
    Q_OBJECT

public:
    bool read(QString fileName);
    bool read(QIODevice *device);

    QString errorString() const;

    bool hasIndex(int index) const;
    int indexOf(QString fileName) const;

    QString fileName(int index) const;
    QByteArray data(QString fileName) const;
    QByteArray data(int index) const;
    qint64 size(int index) const;

    qsizetype fileCount() const;

private:
    struct Entry {
        QString fileName;
        QByteArray data;
    };

    QList<Entry> m_entries;
    QString m_errorString;
};

} // namespace esu

#endif // ESU_CORE_PACKAGE_H
