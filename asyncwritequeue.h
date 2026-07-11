#ifndef ASYNCWRITEQUEUE_H
#define ASYNCWRITEQUEUE_H

#include <QByteArray>
#include <QIODevice>
#include <QList>
#include <QObject>
#include <QTimer>

class AsyncWriteQueue : public QObject
{
    Q_OBJECT

public:
    explicit AsyncWriteQueue(QIODevice *device, QObject *parent = nullptr);

    quint64 enqueue(const QByteArray &data);
    void abortAll(const QString &reason);
    void setTimeoutMs(int timeoutMs);
    int timeoutMs() const;
    int pendingCount() const;

signals:
    void writeCompleted(quint64 id, qint64 bytes);
    void writeFailed(quint64 id, const QString &message);

private slots:
    void onBytesWritten(qint64 bytes);
    void onTimeout();

private:
    struct Request
    {
        quint64 id = 0;
        QByteArray data;
        qint64 submitted = 0;
        qint64 acknowledged = 0;
    };

    void pump();

    QIODevice *m_device = nullptr;
    QList<Request> m_requests;
    QTimer m_timeoutTimer;
    quint64 m_nextId = 1;
    int m_timeoutMs = 1000;
};

#endif // ASYNCWRITEQUEUE_H
