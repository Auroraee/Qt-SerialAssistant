#include "serialportmanager.h"

#include <QSerialPortInfo>

SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
    , m_autoSendTimer(new QTimer(this))
{
    connect(m_serialPort, &QSerialPort::readyRead,
            this, &SerialPortManager::onReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialPortManager::onErrorOccurred);
    connect(m_autoSendTimer, &QTimer::timeout,
            this, &SerialPortManager::onAutoSendTimeout);
}

SerialPortManager::~SerialPortManager()
{
    close();
}

bool SerialPortManager::isOpen() const
{
    return m_serialPort->isOpen();
}

void SerialPortManager::open(const QString &portName, int baudRate)
{
    if (m_serialPort->isOpen()) {
        if (m_serialPort->portName() == portName &&
            m_serialPort->baudRate() == baudRate) {
            return;
        }
        close();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("无法打开串口 %1: %2")
                           .arg(portName, m_serialPort->errorString()));
        return;
    }

    emit connectionStateChanged(true);
}

void SerialPortManager::close()
{
    m_autoSendTimer->stop();
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        emit connectionStateChanged(false);
    }
}

void SerialPortManager::sendData(const QByteArray &data)
{
    if (!m_serialPort->isOpen()) {
        emit errorOccurred(tr("串口未打开"));
        return;
    }

    if (data.isEmpty())
        return;

    qint64 totalWritten = 0;
    while (totalWritten < data.size()) {
        qint64 written = m_serialPort->write(data.mid(totalWritten));
        if (written == -1) {
            emit errorOccurred(tr("发送失败: %1").arg(m_serialPort->errorString()));
            return;
        }
        if (written == 0) {
            if (!m_serialPort->waitForBytesWritten(100)) {
                emit errorOccurred(tr("发送超时: %1").arg(m_serialPort->errorString()));
                return;
            }
            continue;
        }
        totalWritten += written;
    }
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

    emit errorOccurred(m_serialPort->errorString());

    if (error == QSerialPort::ResourceError) {
        close();
    }
}

void SerialPortManager::onAutoSendTimeout()
{
    emit autoSendTriggered();
}
