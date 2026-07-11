#ifndef UTILS_H
#define UTILS_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace Utils {

QByteArray parseHexString(const QString &input, bool *ok = nullptr);
QString toHexString(const QByteArray &data);
QString toAsciiDisplayString(const QByteArray &data);
quint16 crc16Modbus(const QByteArray &data);
int bitCount(const QByteArray &data);
int evenParityBit(const QByteArray &data);
int oddParityBit(const QByteArray &data);

} // namespace Utils

#endif // UTILS_H
