#ifndef SERIALSESSIONCONTROLLER_H
#define SERIALSESSIONCONTROLLER_H

#include "frameassembler.h"
#include "framelogmodel.h"
#include "serialportmanager.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

class SerialSessionController : public QObject
{
    Q_OBJECT

public:
    explicit SerialSessionController(QObject *parent = nullptr);
    explicit SerialSessionController(SerialPortManager *manager, QObject *parent = nullptr);

    SerialPortManager *portManager() const;
    FrameLogModel *logModel() const;
    ConnectionState state() const;
    bool isOpen() const;

    FrameConfig frameConfig() const;
    void setFrameConfig(const FrameConfig &config);

    qint64 rxBytes() const;
    qint64 txBytes() const;

public slots:
    void open(const SerialPortConfig &config);
    void close();
    quint64 send(const QByteArray &data);
    void setAutoSend(bool enabled, int intervalMs, const QByteArray &data);
    void updateAutoSendData(const QByteArray &data);
    void clearCounters();

signals:
    void stateChanged(ConnectionState state);
    void rawDataReceived(const QByteArray &data);
    void countersChanged(qint64 rxBytes, qint64 txBytes);
    void errorOccurred(const QString &message);
    void criticalErrorOccurred(const QString &message);
    void autoSendStopped(const QString &reason);

private slots:
    void onConnectionStateChanged(bool connected);
    void onDataReceived(const QByteArray &data);
    void onIdleGapElapsed();
    void flushLogBatch();
    void onWriteCompleted(quint64 id, qint64 bytes);
    void onWriteFailed(quint64 id, const QString &message);
    void onCriticalError(const QString &message);
    void onAutoSendTimeout();

private:
    void initialize();
    void setState(ConnectionState state);
    void processAssemblyResult(const FrameAssemblyResult &result, FrameDirection direction);
    void queueFrame(const QByteArray &frame, FrameDirection direction, bool automatic = false);
    void queueError(const QString &message);
    void queueRecord(const FrameRecord &record);

    SerialPortManager *m_manager = nullptr;
    FrameLogModel *m_logModel = nullptr;
    FrameAssembler m_assembler;
    ConnectionState m_state = ConnectionState::Disconnected;
    QTimer m_idleGapTimer;
    QTimer m_logFlushTimer;
    QTimer m_autoSendTimer;
    QList<FrameRecord> m_pendingRecords;
    QHash<quint64, QByteArray> m_pendingWrites;
    QSet<quint64> m_automaticWrites;
    QByteArray m_autoSendData;
    qint64 m_rxBytes = 0;
    qint64 m_txBytes = 0;
};

#endif // SERIALSESSIONCONTROLLER_H
