#ifndef FRAMELOGMODEL_H
#define FRAMELOGMODEL_H

#include "frametypes.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

enum class LogExportFormat
{
    Text,
    Csv,
    JsonLines
};

struct FrameFilter
{
    bool directionEnabled = false;
    FrameDirection direction = FrameDirection::Rx;
    QString protocol;
    int station = -1;
    int functionCode = -1;
    bool crcEnabled = false;
    CrcStatus crcStatus = CrcStatus::NotChecked;
    QString keyword;
};

class FrameLogModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        TimestampColumn,
        DirectionColumn,
        HexColumn,
        AsciiColumn,
        ProtocolColumn,
        SummaryColumn,
        CrcColumn,
        ErrorColumn,
        ColumnCount
    };

    explicit FrameLogModel(QObject *parent = nullptr,
                           int maxRecords = 10000,
                           qint64 maxRawBytes = 32LL * 1024 * 1024);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void append(const FrameRecord &record);
    void appendBatch(const QList<FrameRecord> &records);
    void clear();

    FrameRecord recordAt(int row) const;
    QList<FrameRecord> records() const;
    qint64 rawByteCount() const;

    bool exportToFile(const QString &filePath, LogExportFormat format,
                      QString *error = nullptr) const;

private:
    QList<FrameRecord> m_records;
    int m_maxRecords;
    qint64 m_maxRawBytes;
    qint64 m_rawByteCount = 0;
};

class FrameFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit FrameFilterProxyModel(QObject *parent = nullptr);

    void setFrameFilter(const FrameFilter &filter);
    FrameFilter frameFilter() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    FrameFilter m_filter;
};

#endif // FRAMELOGMODEL_H
