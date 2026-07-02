#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>

class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);
    void connectionStateChanged(bool connected);
};

#endif // SERIALPORTMANAGER_H
