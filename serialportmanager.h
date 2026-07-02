#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>

class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

    bool isOpen() const;

public slots:
    void open(const QString &portName, int baudRate);
    void close();
    void sendData(const QByteArray &data);
    void setAutoSendEnabled(bool enabled, int intervalMs);

signals:
    void connectionStateChanged(bool connected);
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
    void autoSendTriggered();

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onAutoSendTimeout();

private:
    QSerialPort *m_serialPort = nullptr;
    QTimer *m_autoSendTimer = nullptr;
};

#endif // SERIALPORTMANAGER_H
