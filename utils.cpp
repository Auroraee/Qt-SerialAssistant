#include "utils.h"

#include <QRegularExpression>

namespace Utils {

QByteArray parseHexString(const QString &input, bool *ok)
{
    if (ok)
        *ok = false;

    QString cleaned = input;
    cleaned.remove(' ');

    if (cleaned.isEmpty()) {
        if (ok)
            *ok = true;
        return QByteArray();
    }

    if (cleaned.length() % 2 != 0)
        return QByteArray();

    static const QRegularExpression hexRe("^[0-9A-Fa-f]+$");
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

} // namespace Utils
