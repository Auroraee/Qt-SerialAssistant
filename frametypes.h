#ifndef FRAMETYPES_H
#define FRAMETYPES_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QVariantMap>

enum class ConnectionState
{
    Disconnected,
    Opening,
    Connected,
    Closing,
    Error
};

enum class FramingMode
{
    IdleGap,
    Delimiter,
    FixedLength
};

enum class FrameDirection
{
    Rx,
    Tx,
    Error
};

enum class CrcStatus
{
    NotChecked,
    Valid,
    Invalid
};

struct FrameConfig
{
    FramingMode mode = FramingMode::IdleGap;
    QByteArray delimiter = QByteArray("\r\n");
    int fixedLength = 8;
    int idleGapMs = 20;
    qsizetype maxFrameSize = 64 * 1024;
    bool includeDelimiter = false;
};

struct FrameAssemblyResult
{
    QList<QByteArray> frames;
    QString error;
};

struct DecodeResult
{
    QString protocol;
    QString summary;
    CrcStatus crcStatus = CrcStatus::NotChecked;
    QVariantMap fields;
    QString error;
};

struct FrameRecord
{
    QDateTime timestamp;
    FrameDirection direction = FrameDirection::Rx;
    QByteArray rawData;
    QString protocol;
    QString summary;
    CrcStatus crcStatus = CrcStatus::NotChecked;
    QVariantMap fields;
    QString error;
};

Q_DECLARE_METATYPE(ConnectionState)
Q_DECLARE_METATYPE(FrameRecord)

#endif // FRAMETYPES_H
