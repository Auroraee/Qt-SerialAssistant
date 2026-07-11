#include "asyncwritequeue.h"

AsyncWriteQueue::AsyncWriteQueue(QIODevice *device, QObject *parent)
    : QObject(parent)
    , m_device(device)
{
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &AsyncWriteQueue::onTimeout);
    if (m_device) {
        connect(m_device, &QIODevice::bytesWritten,
                this, &AsyncWriteQueue::onBytesWritten);
        connect(m_device, &QObject::destroyed, this, [this] {
            m_device = nullptr;
            abortAll(tr("写入设备已销毁"));
        });
    }
}

quint64 AsyncWriteQueue::enqueue(const QByteArray &data)
{
    const quint64 id = m_nextId++;
    if (data.isEmpty()) {
        emit writeCompleted(id, 0);
        return id;
    }
    if (!m_device || !m_device->isOpen() || !m_device->isWritable()) {
        emit writeFailed(id, tr("写入设备未打开"));
        return id;
    }

    Request request;
    request.id = id;
    request.data = data;
    m_requests.append(request);
    if (m_requests.size() == 1)
        pump();
    return id;
}

void AsyncWriteQueue::abortAll(const QString &reason)
{
    m_timeoutTimer.stop();
    const QList<Request> pending = m_requests;
    m_requests.clear();
    for (const Request &request : pending)
        emit writeFailed(request.id, reason);
}

void AsyncWriteQueue::setTimeoutMs(int timeoutMs)
{
    m_timeoutMs = qMax(1, timeoutMs);
    if (m_timeoutTimer.isActive())
        m_timeoutTimer.start(m_timeoutMs);
}

int AsyncWriteQueue::timeoutMs() const
{
    return m_timeoutMs;
}

int AsyncWriteQueue::pendingCount() const
{
    return m_requests.size();
}

void AsyncWriteQueue::onBytesWritten(qint64 bytes)
{
    if (m_requests.isEmpty() || bytes <= 0)
        return;

    Request &request = m_requests.first();
    request.acknowledged = qMin(request.submitted, request.acknowledged + bytes);
    m_timeoutTimer.start(m_timeoutMs);

    if (request.acknowledged >= request.data.size()) {
        const quint64 id = request.id;
        const qint64 size = request.data.size();
        m_requests.removeFirst();
        m_timeoutTimer.stop();
        emit writeCompleted(id, size);
        pump();
    } else if (request.acknowledged == request.submitted) {
        pump();
    }
}

void AsyncWriteQueue::onTimeout()
{
    abortAll(tr("写入超时"));
}

void AsyncWriteQueue::pump()
{
    if (m_requests.isEmpty()) {
        m_timeoutTimer.stop();
        return;
    }
    if (!m_device || !m_device->isOpen() || !m_device->isWritable()) {
        abortAll(tr("写入设备未打开"));
        return;
    }

    Request &request = m_requests.first();
    if (request.submitted > request.acknowledged)
        return;
    if (request.submitted >= request.data.size())
        return;

    const qint64 written = m_device->write(request.data.constData() + request.submitted,
                                           request.data.size() - request.submitted);
    if (written < 0) {
        const QString message = m_device->errorString().isEmpty()
                                    ? tr("写入失败")
                                    : m_device->errorString();
        abortAll(message);
        return;
    }
    request.submitted += written;
    m_timeoutTimer.start(m_timeoutMs);
}
