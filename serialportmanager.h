#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>

class AsyncWriteQueue;

struct SerialPortConfig
{
    QString portName;
    qint32 baudRate = QSerialPort::Baud115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
};

class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

    bool isOpen() const;

public slots:
    void open(const SerialPortConfig &config);
    void close();
    quint64 enqueueWrite(const QByteArray &data);
    bool sendData(const QByteArray &data);
    void setAutoSendEnabled(bool enabled, int intervalMs);

signals:
    void connectionStateChanged(bool connected);
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
    void criticalErrorOccurred(const QString &message);
    void writeCompleted(quint64 id, qint64 bytes);
    void writeFailed(quint64 id, const QString &message);
    void autoSendTriggered();

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onAutoSendTimeout();

private:
    QSerialPort *m_serialPort = nullptr;
    AsyncWriteQueue *m_writeQueue = nullptr;
    QTimer *m_autoSendTimer = nullptr;
};

#endif // SERIALPORTMANAGER_H
