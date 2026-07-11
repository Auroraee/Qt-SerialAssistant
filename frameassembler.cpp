#include "frameassembler.h"

FrameAssembler::FrameAssembler(const FrameConfig &config)
    : m_config(config)
{
}

void FrameAssembler::setConfig(const FrameConfig &config)
{
    m_config = config;
    m_buffer.clear();
}

FrameConfig FrameAssembler::config() const
{
    return m_config;
}

FrameAssemblyResult FrameAssembler::append(const QByteArray &data)
{
    FrameAssemblyResult result;
    result.error = validateConfig();
    if (!result.error.isEmpty()) {
        m_buffer.clear();
        return result;
    }

    if (data.isEmpty())
        return result;

    m_buffer.append(data);

    if (m_config.mode == FramingMode::FixedLength) {
        while (m_buffer.size() >= m_config.fixedLength) {
            result.frames.append(m_buffer.left(m_config.fixedLength));
            m_buffer.remove(0, m_config.fixedLength);
        }
    } else if (m_config.mode == FramingMode::Delimiter) {
        qsizetype delimiterIndex = m_buffer.indexOf(m_config.delimiter);
        while (delimiterIndex >= 0) {
            const qsizetype consumedLength = delimiterIndex + m_config.delimiter.size();
            if (consumedLength > m_config.maxFrameSize) {
                result.error = QStringLiteral("Frame exceeded %1 bytes and was discarded")
                                   .arg(m_config.maxFrameSize);
                m_buffer.remove(0, consumedLength);
                delimiterIndex = m_buffer.indexOf(m_config.delimiter);
                continue;
            }
            const qsizetype outputLength = delimiterIndex
                                               + (m_config.includeDelimiter
                                                      ? m_config.delimiter.size()
                                                      : 0);
            result.frames.append(m_buffer.left(outputLength));
            m_buffer.remove(0, consumedLength);
            delimiterIndex = m_buffer.indexOf(m_config.delimiter);
        }
    }

    if (m_buffer.size() > m_config.maxFrameSize) {
        result.error = QStringLiteral("Frame buffer exceeded %1 bytes and was cleared")
                           .arg(m_config.maxFrameSize);
        m_buffer.clear();
    }

    return result;
}

FrameAssemblyResult FrameAssembler::flush()
{
    FrameAssemblyResult result;
    result.error = validateConfig();
    if (!result.error.isEmpty()) {
        m_buffer.clear();
        return result;
    }

    if (!m_buffer.isEmpty()) {
        result.frames.append(m_buffer);
        m_buffer.clear();
    }
    return result;
}

void FrameAssembler::clear()
{
    m_buffer.clear();
}

QByteArray FrameAssembler::bufferedData() const
{
    return m_buffer;
}

QString FrameAssembler::validateConfig() const
{
    if (m_config.maxFrameSize <= 0)
        return QStringLiteral("Maximum frame size must be greater than zero");
    if (m_config.mode == FramingMode::FixedLength) {
        if (m_config.fixedLength <= 0)
            return QStringLiteral("Fixed frame length must be greater than zero");
        if (m_config.fixedLength > m_config.maxFrameSize)
            return QStringLiteral("Fixed frame length exceeds maximum frame size");
    }
    if (m_config.mode == FramingMode::Delimiter && m_config.delimiter.isEmpty())
        return QStringLiteral("Frame delimiter cannot be empty");
    return QString();
}
