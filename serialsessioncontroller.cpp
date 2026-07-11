#include "serialsessioncontroller.h"

#include "modbusrtudecoder.h"

SerialSessionController::SerialSessionController(QObject *parent)
    : QObject(parent)
    , m_manager(new SerialPortManager(this))
    , m_logModel(new FrameLogModel(this))
{
    initialize();
}

SerialSessionController::SerialSessionController(SerialPortManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager ? manager : new SerialPortManager(this))
    , m_logModel(new FrameLogModel(this))
{
    initialize();
}

void SerialSessionController::initialize()
{
    m_idleGapTimer.setSingleShot(true);
    m_logFlushTimer.setSingleShot(true);
    m_logFlushTimer.setInterval(50);

    connect(m_manager, &SerialPortManager::connectionStateChanged,
            this, &SerialSessionController::onConnectionStateChanged);
    connect(m_manager, &SerialPortManager::dataReceived,
            this, &SerialSessionController::onDataReceived);
    connect(m_manager, &SerialPortManager::writeCompleted,
            this, &SerialSessionController::onWriteCompleted);
    connect(m_manager, &SerialPortManager::writeFailed,
            this, &SerialSessionController::onWriteFailed);
    connect(m_manager, &SerialPortManager::errorOccurred,
            this, &SerialSessionController::errorOccurred);
    connect(m_manager, &SerialPortManager::criticalErrorOccurred,
            this, &SerialSessionController::onCriticalError);
    connect(&m_idleGapTimer, &QTimer::timeout,
            this, &SerialSessionController::onIdleGapElapsed);
    connect(&m_logFlushTimer, &QTimer::timeout,
            this, &SerialSessionController::flushLogBatch);
    connect(&m_autoSendTimer, &QTimer::timeout,
            this, &SerialSessionController::onAutoSendTimeout);
}

SerialPortManager *SerialSessionController::portManager() const
{
    return m_manager;
}

FrameLogModel *SerialSessionController::logModel() const
{
    return m_logModel;
}

ConnectionState SerialSessionController::state() const
{
    return m_state;
}

bool SerialSessionController::isOpen() const
{
    return m_manager->isOpen();
}

FrameConfig SerialSessionController::frameConfig() const
{
    return m_assembler.config();
}

void SerialSessionController::setFrameConfig(const FrameConfig &config)
{
    m_idleGapTimer.stop();
    m_assembler.setConfig(config);
}

qint64 SerialSessionController::rxBytes() const
{
    return m_rxBytes;
}

qint64 SerialSessionController::txBytes() const
{
    return m_txBytes;
}

void SerialSessionController::open(const SerialPortConfig &config)
{
    if (isOpen())
        return;
    setState(ConnectionState::Opening);
    m_manager->open(config);
}

void SerialSessionController::close()
{
    if (!isOpen()) {
        setState(ConnectionState::Disconnected);
        return;
    }
    setState(ConnectionState::Closing);
    m_manager->close();
}

quint64 SerialSessionController::send(const QByteArray &data)
{
    if (data.isEmpty()) {
        const QString message = tr("发送内容为空");
        queueError(message);
        emit errorOccurred(message);
        return 0;
    }
    if (!isOpen()) {
        const QString message = tr("串口未打开");
        queueError(message);
        emit errorOccurred(message);
        return 0;
    }

    const quint64 id = m_manager->enqueueWrite(data);
    m_pendingWrites.insert(id, data);
    return id;
}

void SerialSessionController::setAutoSend(bool enabled, int intervalMs,
                                          const QByteArray &data)
{
    m_autoSendTimer.stop();
    if (!enabled)
        return;
    if (!isOpen()) {
        const QString message = tr("串口未打开，无法启动定时发送");
        emit errorOccurred(message);
        emit autoSendStopped(message);
        return;
    }
    if (intervalMs <= 0 || data.isEmpty()) {
        const QString message = tr("定时发送参数无效");
        emit errorOccurred(message);
        emit autoSendStopped(message);
        return;
    }
    m_autoSendData = data;
    m_autoSendTimer.start(intervalMs);
}

void SerialSessionController::updateAutoSendData(const QByteArray &data)
{
    m_autoSendData = data;
}

void SerialSessionController::clearCounters()
{
    m_rxBytes = 0;
    m_txBytes = 0;
    emit countersChanged(m_rxBytes, m_txBytes);
}

void SerialSessionController::onConnectionStateChanged(bool connected)
{
    setState(connected ? ConnectionState::Connected : ConnectionState::Disconnected);
    if (!connected) {
        m_idleGapTimer.stop();
        processAssemblyResult(m_assembler.flush(), FrameDirection::Rx);
        if (m_autoSendTimer.isActive()) {
            m_autoSendTimer.stop();
            emit autoSendStopped(tr("串口已断开"));
        }
    }
}

void SerialSessionController::onDataReceived(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    m_rxBytes += data.size();
    emit countersChanged(m_rxBytes, m_txBytes);
    emit rawDataReceived(data);

    const FrameAssemblyResult result = m_assembler.append(data);
    processAssemblyResult(result, FrameDirection::Rx);

    const FrameConfig config = m_assembler.config();
    if (config.mode == FramingMode::IdleGap && !m_assembler.bufferedData().isEmpty())
        m_idleGapTimer.start(qMax(1, config.idleGapMs));
}

void SerialSessionController::onIdleGapElapsed()
{
    processAssemblyResult(m_assembler.flush(), FrameDirection::Rx);
}

void SerialSessionController::flushLogBatch()
{
    if (m_pendingRecords.isEmpty())
        return;
    const QList<FrameRecord> records = m_pendingRecords;
    m_pendingRecords.clear();
    m_logModel->appendBatch(records);
}

void SerialSessionController::onWriteCompleted(quint64 id, qint64 bytes)
{
    const QByteArray data = m_pendingWrites.take(id);
    const bool automatic = m_automaticWrites.remove(id) > 0;
    if (data.isEmpty() && bytes > 0)
        return;
    m_txBytes += bytes;
    emit countersChanged(m_rxBytes, m_txBytes);
    queueFrame(data.left(bytes), FrameDirection::Tx, automatic);
}

void SerialSessionController::onWriteFailed(quint64 id, const QString &message)
{
    m_pendingWrites.remove(id);
    m_automaticWrites.remove(id);
    queueError(message);
}

void SerialSessionController::onCriticalError(const QString &message)
{
    setState(ConnectionState::Error);
    m_autoSendTimer.stop();
    queueError(message);
    emit criticalErrorOccurred(message);
}

void SerialSessionController::onAutoSendTimeout()
{
    if (m_autoSendData.isEmpty()) {
        m_autoSendTimer.stop();
        emit autoSendStopped(tr("发送内容为空"));
        return;
    }
    const quint64 id = send(m_autoSendData);
    if (id != 0)
        m_automaticWrites.insert(id);
}

void SerialSessionController::setState(ConnectionState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

void SerialSessionController::processAssemblyResult(const FrameAssemblyResult &result,
                                                     FrameDirection direction)
{
    if (!result.error.isEmpty())
        queueError(result.error);
    for (const QByteArray &frame : result.frames)
        queueFrame(frame, direction);
}

void SerialSessionController::queueFrame(const QByteArray &frame, FrameDirection direction,
                                         bool automatic)
{
    if (frame.isEmpty())
        return;
    const DecodeResult decoded = ModbusRtuDecoder::decode(frame, direction);
    FrameRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.direction = direction;
    record.rawData = frame;
    record.protocol = decoded.protocol;
    record.summary = decoded.summary;
    record.crcStatus = decoded.crcStatus;
    record.fields = decoded.fields;
    if (automatic)
        record.fields.insert(QStringLiteral("automatic"), true);
    record.error = decoded.protocol == QLatin1String("Modbus RTU") ? decoded.error : QString();
    queueRecord(record);
}

void SerialSessionController::queueError(const QString &message)
{
    FrameRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.direction = FrameDirection::Error;
    record.protocol = QStringLiteral("System");
    record.summary = message;
    record.error = message;
    queueRecord(record);
}

void SerialSessionController::queueRecord(const FrameRecord &record)
{
    m_pendingRecords.append(record);
    if (!m_logFlushTimer.isActive())
        m_logFlushTimer.start();
}
