#include "framelogmodel.h"

#include "utils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

namespace {
QString directionText(FrameDirection direction)
{
    switch (direction) {
    case FrameDirection::Rx:
        return QStringLiteral("RX");
    case FrameDirection::Tx:
        return QStringLiteral("TX");
    case FrameDirection::Error:
        return QStringLiteral("ERR");
    }
    return QString();
}

QString crcText(CrcStatus status)
{
    switch (status) {
    case CrcStatus::Valid:
        return QStringLiteral("OK");
    case CrcStatus::Invalid:
        return QStringLiteral("BAD");
    case CrcStatus::NotChecked:
        return QStringLiteral("--");
    }
    return QString();
}

QString timestampText(const QDateTime &timestamp)
{
    return timestamp.isValid() ? timestamp.toString(Qt::ISODateWithMs) : QString();
}

QString csvField(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}
}

FrameLogModel::FrameLogModel(QObject *parent, int maxRecords, qint64 maxRawBytes)
    : QAbstractTableModel(parent)
    , m_maxRecords(qMax(1, maxRecords))
    , m_maxRawBytes(qMax<qint64>(1, maxRawBytes))
{
}

int FrameLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

int FrameLogModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant FrameLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return QVariant();

    const FrameRecord &record = m_records.at(index.row());
    if (role == Qt::UserRole)
        return QVariant::fromValue(record);
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return QVariant();

    switch (index.column()) {
    case TimestampColumn:
        return timestampText(record.timestamp);
    case DirectionColumn:
        return directionText(record.direction);
    case HexColumn:
        return Utils::toHexString(record.rawData);
    case AsciiColumn:
        return Utils::toAsciiDisplayString(record.rawData);
    case ProtocolColumn:
        return record.protocol;
    case SummaryColumn:
        return record.summary;
    case CrcColumn:
        return crcText(record.crcStatus);
    case ErrorColumn:
        return record.error;
    default:
        return QVariant();
    }
}

QVariant FrameLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case TimestampColumn:
        return tr("时间");
    case DirectionColumn:
        return tr("方向");
    case HexColumn:
        return QStringLiteral("HEX");
    case AsciiColumn:
        return QStringLiteral("ASCII");
    case ProtocolColumn:
        return tr("协议");
    case SummaryColumn:
        return tr("摘要");
    case CrcColumn:
        return QStringLiteral("CRC");
    case ErrorColumn:
        return tr("错误");
    default:
        return QVariant();
    }
}

void FrameLogModel::append(const FrameRecord &record)
{
    appendBatch({record});
}

void FrameLogModel::appendBatch(const QList<FrameRecord> &records)
{
    if (records.isEmpty())
        return;

    QList<FrameRecord> accepted = records;
    qint64 acceptedBytes = 0;
    for (const FrameRecord &record : accepted)
        acceptedBytes += record.rawData.size();

    while (!accepted.isEmpty()
           && (accepted.size() > m_maxRecords || acceptedBytes > m_maxRawBytes)) {
        acceptedBytes -= accepted.constFirst().rawData.size();
        accepted.removeFirst();
    }
    if (accepted.isEmpty())
        return;

    int removeCount = 0;
    qint64 bytesAfterRemoval = m_rawByteCount;
    while (removeCount < m_records.size()
           && (m_records.size() - removeCount + accepted.size() > m_maxRecords
               || bytesAfterRemoval + acceptedBytes > m_maxRawBytes)) {
        bytesAfterRemoval -= m_records.at(removeCount).rawData.size();
        ++removeCount;
    }

    if (removeCount > 0) {
        beginRemoveRows(QModelIndex(), 0, removeCount - 1);
        for (int i = 0; i < removeCount; ++i)
            m_records.removeFirst();
        m_rawByteCount = bytesAfterRemoval;
        endRemoveRows();
    }

    const int firstNewRow = m_records.size();
    beginInsertRows(QModelIndex(), firstNewRow, firstNewRow + accepted.size() - 1);
    m_records.append(accepted);
    m_rawByteCount += acceptedBytes;
    endInsertRows();
}

void FrameLogModel::clear()
{
    if (m_records.isEmpty())
        return;
    beginResetModel();
    m_records.clear();
    m_rawByteCount = 0;
    endResetModel();
}

FrameRecord FrameLogModel::recordAt(int row) const
{
    if (row < 0 || row >= m_records.size())
        return FrameRecord();
    return m_records.at(row);
}

QList<FrameRecord> FrameLogModel::records() const
{
    return m_records;
}

qint64 FrameLogModel::rawByteCount() const
{
    return m_rawByteCount;
}

bool FrameLogModel::exportToFile(const QString &filePath, LogExportFormat format,
                                 QString *error) const
{
    if (error)
        error->clear();

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    if (format == LogExportFormat::Csv) {
        stream << "timestamp,direction,rawHex,ascii,protocol,summary,crc,error\n";
    }

    for (const FrameRecord &record : m_records) {
        const QString rawHex = Utils::toHexString(record.rawData);
        const QString ascii = Utils::toAsciiDisplayString(record.rawData);
        if (format == LogExportFormat::Text) {
            stream << '[' << timestampText(record.timestamp) << "] "
                   << directionText(record.direction) << ' ' << rawHex;
            if (!record.protocol.isEmpty())
                stream << " | " << record.protocol;
            if (!record.summary.isEmpty())
                stream << " | " << record.summary;
            if (record.crcStatus != CrcStatus::NotChecked)
                stream << " | CRC=" << crcText(record.crcStatus);
            if (!record.error.isEmpty())
                stream << " | " << record.error;
            stream << '\n';
        } else if (format == LogExportFormat::Csv) {
            const QStringList fields = {
                timestampText(record.timestamp), directionText(record.direction), rawHex, ascii,
                record.protocol, record.summary, crcText(record.crcStatus), record.error
            };
            QStringList escaped;
            escaped.reserve(fields.size());
            for (const QString &field : fields)
                escaped.append(csvField(field));
            stream << escaped.join(QLatin1Char(',')) << '\n';
        } else {
            QJsonObject object;
            object.insert(QStringLiteral("timestamp"), timestampText(record.timestamp));
            object.insert(QStringLiteral("direction"), directionText(record.direction));
            object.insert(QStringLiteral("rawHex"),
                          QString::fromLatin1(record.rawData.toHex().toUpper()));
            object.insert(QStringLiteral("ascii"), ascii);
            object.insert(QStringLiteral("protocol"), record.protocol);
            object.insert(QStringLiteral("summary"), record.summary);
            object.insert(QStringLiteral("crc"), crcText(record.crcStatus));
            object.insert(QStringLiteral("error"), record.error);
            object.insert(QStringLiteral("fields"), QJsonObject::fromVariantMap(record.fields));
            stream << QJsonDocument(object).toJson(QJsonDocument::Compact) << '\n';
        }
    }

    if (stream.status() != QTextStream::Ok) {
        if (error)
            *error = tr("写入日志失败");
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

FrameFilterProxyModel::FrameFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void FrameFilterProxyModel::setFrameFilter(const FrameFilter &filter)
{
    m_filter = filter;
    invalidateFilter();
}

FrameFilter FrameFilterProxyModel::frameFilter() const
{
    return m_filter;
}

bool FrameFilterProxyModel::filterAcceptsRow(int sourceRow,
                                             const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent)
    const auto *model = qobject_cast<const FrameLogModel *>(sourceModel());
    if (!model)
        return true;

    const FrameRecord record = model->recordAt(sourceRow);
    if (m_filter.directionEnabled && record.direction != m_filter.direction)
        return false;
    if (!m_filter.protocol.isEmpty()
        && record.protocol.compare(m_filter.protocol, Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (m_filter.station >= 0
        && record.fields.value(QStringLiteral("station"), -1).toInt() != m_filter.station) {
        return false;
    }
    if (m_filter.functionCode >= 0
        && record.fields.value(QStringLiteral("functionCode"), -1).toInt()
               != m_filter.functionCode) {
        return false;
    }
    if (m_filter.crcEnabled && record.crcStatus != m_filter.crcStatus)
        return false;

    const QString keyword = m_filter.keyword.trimmed();
    if (!keyword.isEmpty()) {
        QString searchable = record.protocol + QLatin1Char(' ') + record.summary
                             + QLatin1Char(' ') + record.error + QLatin1Char(' ')
                             + Utils::toHexString(record.rawData) + QLatin1Char(' ')
                             + Utils::toAsciiDisplayString(record.rawData);
        for (auto it = record.fields.cbegin(); it != record.fields.cend(); ++it)
            searchable += QLatin1Char(' ') + it.key() + QLatin1Char(' ') + it.value().toString();
        if (!searchable.contains(keyword, Qt::CaseInsensitive))
            return false;
    }
    return true;
}
