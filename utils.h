#ifndef UTILS_H
#define UTILS_H

#include <QByteArray>
#include <QString>

namespace Utils {

QByteArray parseHexString(const QString &input, bool *ok = nullptr);
QString toHexString(const QByteArray &data);
QString toAsciiDisplayString(const QByteArray &data);

} // namespace Utils

#endif // UTILS_H
