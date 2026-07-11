#include "serialportmanager.h"

#include "asyncwritequeue.h"

#include <QSignalBlocker>

SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
    , m_writeQueue(new AsyncWriteQueue(m_serialPort, this))
    , m_autoSendTimer(new QTimer(this))
{
    connect(m_serialPort, &QSerialPort::readyRead,
            this, &SerialPortManager::onReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialPortManager::onErrorOccurred);
    connect(m_autoSendTimer, &QTimer::timeout,
            this, &SerialPortManager::onAutoSendTimeout);
    connect(m_writeQueue, &AsyncWriteQueue::writeCompleted,
            this, &SerialPortManager::writeCompleted);
    connect(m_writeQueue, &AsyncWriteQueue::writeFailed,
            this, [this](quint64 id, const QString &message) {
                emit writeFailed(id, message);
                emit errorOccurred(message);
            });
}

SerialPortManager::~SerialPortManager()
{
    close();
}

bool SerialPortManager::isOpen() const
{
    return m_serialPort->isOpen();
}

void SerialPortManager::open(const SerialPortConfig &config)
{
    if (m_serialPort->isOpen()) {
        if (m_serialPort->portName() == config.portName &&
            m_serialPort->baudRate() == config.baudRate &&
            m_serialPort->dataBits() == config.dataBits &&
            m_serialPort->parity() == config.parity &&
            m_serialPort->stopBits() == config.stopBits &&
            m_serialPort->flowControl() == config.flowControl) {
            return;
        }
        close();
    }

    m_serialPort->setPortName(config.portName);
    m_serialPort->setBaudRate(config.baudRate);
    m_serialPort->setDataBits(config.dataBits);
    m_serialPort->setParity(config.parity);
    m_serialPort->setStopBits(config.stopBits);
    m_serialPort->setFlowControl(config.flowControl);

    QSignalBlocker blocker(m_serialPort);
    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        const QString message = tr("无法打开串口 %1: %2")
                                    .arg(config.portName, m_serialPort->errorString());
        emit errorOccurred(message);
        emit criticalErrorOccurred(message);
        return;
    }

    emit connectionStateChanged(true);
}

void SerialPortManager::close()
{
    m_autoSendTimer->stop();
    if (m_writeQueue->pendingCount() > 0)
        m_writeQueue->abortAll(tr("串口已关闭，待发送数据已取消"));
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        emit connectionStateChanged(false);
    }
}

quint64 SerialPortManager::enqueueWrite(const QByteArray &data)
{
    return m_writeQueue->enqueue(data);
}

bool SerialPortManager::sendData(const QByteArray &data)
{
    if (!m_serialPort->isOpen()) {
        emit errorOccurred(tr("串口未打开"));
        return false;
    }

    enqueueWrite(data);
    return true;
}

void SerialPortManager::setAutoSendEnabled(bool enabled, int intervalMs)
{
    m_autoSendTimer->stop();
    if (!enabled)
        return;

    if (intervalMs <= 0) {
        emit errorOccurred(tr("定时发送间隔必须大于 0 ms"));
        return;
    }

    if (!m_serialPort->isOpen()) {
        emit errorOccurred(tr("串口未打开，无法启动定时发送"));
        return;
    }

    m_autoSendTimer->setInterval(intervalMs);
    m_autoSendTimer->start();
}

void SerialPortManager::onReadyRead()
{
    QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialPortManager::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    const QString message = m_serialPort->errorString();
    emit errorOccurred(message);

    if (error == QSerialPort::ResourceError) {
        emit criticalErrorOccurred(message);
        close();
    }
}

void SerialPortManager::onAutoSendTimeout()
{
    emit autoSendTriggered();
}
