#include "utils.h"

#include <QRegularExpression>

namespace Utils {

QByteArray parseHexString(const QString &input, bool *ok)
{
    if (ok)
        *ok = false;

    // 每个 token 独立处理 0x 前缀和奇数位补零。
    const QStringList tokens = input.split(QRegularExpression(QStringLiteral("\\s+")),
                                           Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        if (ok)
            *ok = true;
        return QByteArray();
    }

    QString cleaned;
    for (const QString &token : tokens) {
        QString t = token;
        if (t.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            t.remove(0, 2);
        if (t.isEmpty())
            return QByteArray();
        if (t.length() % 2 != 0)
            t.prepend(QLatin1Char('0'));
        cleaned += t;
    }

    if (cleaned.isEmpty()) {
        if (ok)
            *ok = true;
        return QByteArray();
    }

    static const QRegularExpression hexRe(QStringLiteral("^[0-9A-Fa-f]+$"));
    if (!hexRe.match(cleaned).hasMatch())
        return QByteArray();

    QByteArray result;
    result.reserve(cleaned.length() / 2);
    for (int i = 0; i < cleaned.length(); i += 2) {
        bool byteOk = false;
        const int byteValue = cleaned.mid(i, 2).toInt(&byteOk, 16);
        if (!byteOk)
            return QByteArray();
        result.append(static_cast<char>(byteValue));
    }

    if (ok)
        *ok = true;
    return result;
}

QString toHexString(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

QString toAsciiDisplayString(const QByteArray &data)
{
    QString result;
    result.reserve(data.size());
    for (char c : data) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 0x20 && uc <= 0x7E)
            result.append(QChar(uc));
        else
            result.append('.');
    }
    return result;
}

quint16 crc16Modbus(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (char c : data) {
        crc ^= static_cast<quint8>(c);
        for (int i = 0; i < 8; ++i) {
            if ((crc & 0x0001) != 0)
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            else
                crc = static_cast<quint16>(crc >> 1);
        }
    }
    return crc;
}

int bitCount(const QByteArray &data)
{
    int count = 0;
    for (char c : data) {
        quint8 value = static_cast<quint8>(c);
        while (value != 0) {
            count += value & 0x01;
            value = static_cast<quint8>(value >> 1);
        }
    }
    return count;
}

int evenParityBit(const QByteArray &data)
{
    return bitCount(data) % 2;
}

int oddParityBit(const QByteArray &data)
{
    return 1 - evenParityBit(data);
}

} // namespace Utils
