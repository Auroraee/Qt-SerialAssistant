#include "modbusrtudecoder.h"

#include "utils.h"

namespace {
quint8 byteAt(const QByteArray &data, qsizetype index)
{
    return static_cast<quint8>(data.at(index));
}

quint16 wordAt(const QByteArray &data, qsizetype index)
{
    return static_cast<quint16>((quint16(byteAt(data, index)) << 8)
                                | quint16(byteAt(data, index + 1)));
}

bool isSupportedFunction(quint8 functionCode)
{
    switch (functionCode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x0F:
    case 0x10:
        return true;
    default:
        return false;
    }
}

QString functionText(quint8 functionCode)
{
    return QStringLiteral("0x%1").arg(functionCode, 2, 16, QLatin1Char('0')).toUpper();
}

QString roleForDirection(FrameDirection direction)
{
    return direction == FrameDirection::Tx ? QStringLiteral("request")
                                            : QStringLiteral("response");
}
}

DecodeResult ModbusRtuDecoder::decode(const QByteArray &frame, FrameDirection direction)
{
    DecodeResult result;
    result.protocol = QStringLiteral("Raw");

    if (frame.size() < 2) {
        result.error = QStringLiteral("Frame is too short to identify a protocol");
        return result;
    }

    const quint8 wireFunction = byteAt(frame, 1);
    const quint8 functionCode = wireFunction & 0x7F;
    if (!isSupportedFunction(functionCode)) {
        result.error = QStringLiteral("Unsupported Modbus function code %1")
                           .arg(functionText(functionCode));
        return result;
    }

    result.protocol = QStringLiteral("Modbus RTU");
    result.fields.insert(QStringLiteral("station"), int(byteAt(frame, 0)));
    result.fields.insert(QStringLiteral("functionCode"), int(functionCode));

    if (frame.size() < 4) {
        result.crcStatus = CrcStatus::Invalid;
        result.error = QStringLiteral("Truncated Modbus RTU frame");
        return result;
    }

    const QByteArray payload = frame.left(frame.size() - 2);
    const quint16 receivedCrc = quint16(byteAt(frame, frame.size() - 2))
                                | (quint16(byteAt(frame, frame.size() - 1)) << 8);
    const quint16 calculatedCrc = Utils::crc16Modbus(payload);
    if (receivedCrc != calculatedCrc) {
        result.crcStatus = CrcStatus::Invalid;
        result.error = QStringLiteral("CRC mismatch: expected 0x%1, received 0x%2")
                           .arg(calculatedCrc, 4, 16, QLatin1Char('0'))
                           .arg(receivedCrc, 4, 16, QLatin1Char('0'))
                           .toUpper();
        return result;
    }
    result.crcStatus = CrcStatus::Valid;

    const int station = byteAt(payload, 0);
    if ((wireFunction & 0x80) != 0) {
        if (payload.size() != 3) {
            result.error = QStringLiteral("Invalid Modbus exception response length");
            return result;
        }
        const int exceptionCode = byteAt(payload, 2);
        result.fields.insert(QStringLiteral("role"), QStringLiteral("exception"));
        result.fields.insert(QStringLiteral("exceptionCode"), exceptionCode);
        result.summary = QStringLiteral("Station %1 exception FC=%2 code=0x%3")
                             .arg(station)
                             .arg(functionText(functionCode))
                             .arg(exceptionCode, 2, 16, QLatin1Char('0'))
                             .toUpper();
        return result;
    }

    QString role = roleForDirection(direction);

    if (functionCode >= 0x01 && functionCode <= 0x04) {
        const bool requestShape = payload.size() == 6;
        const bool responseShape = payload.size() >= 3
                                   && byteAt(payload, 2) == payload.size() - 3;
        if (requestShape != responseShape)
            role = requestShape ? QStringLiteral("request") : QStringLiteral("response");
        result.fields.insert(QStringLiteral("role"), role);
        if (role == QLatin1String("request")) {
            if (payload.size() != 6) {
                result.error = QStringLiteral("Invalid Modbus read request length");
                return result;
            }
            result.fields.insert(QStringLiteral("startAddress"), int(wordAt(payload, 2)));
            result.fields.insert(QStringLiteral("quantity"), int(wordAt(payload, 4)));
        } else {
            if (payload.size() < 3) {
                result.error = QStringLiteral("Truncated Modbus read response");
                return result;
            }
            const int byteCount = byteAt(payload, 2);
            if (payload.size() != 3 + byteCount) {
                result.error = QStringLiteral("Modbus response byte count does not match frame length");
                return result;
            }
            result.fields.insert(QStringLiteral("byteCount"), byteCount);
            result.fields.insert(QStringLiteral("valuesHex"),
                                 Utils::toHexString(payload.mid(3, byteCount)));
        }
    } else if (functionCode == 0x05 || functionCode == 0x06) {
        result.fields.insert(QStringLiteral("role"), role);
        if (payload.size() != 6) {
            result.error = QStringLiteral("Invalid Modbus single-write frame length");
            return result;
        }
        result.fields.insert(QStringLiteral("address"), int(wordAt(payload, 2)));
        result.fields.insert(QStringLiteral("value"), int(wordAt(payload, 4)));
    } else if (functionCode == 0x0F || functionCode == 0x10) {
        const bool requestShape = payload.size() >= 7
                                  && byteAt(payload, 6) == payload.size() - 7;
        const bool responseShape = payload.size() == 6;
        if (requestShape != responseShape)
            role = requestShape ? QStringLiteral("request") : QStringLiteral("response");
        result.fields.insert(QStringLiteral("role"), role);
        if (role == QLatin1String("request")) {
            if (payload.size() < 7) {
                result.error = QStringLiteral("Truncated Modbus multiple-write request");
                return result;
            }
            const int byteCount = byteAt(payload, 6);
            if (payload.size() != 7 + byteCount) {
                result.error = QStringLiteral("Modbus write byte count does not match frame length");
                return result;
            }
            result.fields.insert(QStringLiteral("startAddress"), int(wordAt(payload, 2)));
            result.fields.insert(QStringLiteral("quantity"), int(wordAt(payload, 4)));
            result.fields.insert(QStringLiteral("byteCount"), byteCount);
            result.fields.insert(QStringLiteral("valuesHex"),
                                 Utils::toHexString(payload.mid(7, byteCount)));
        } else {
            if (payload.size() != 6) {
                result.error = QStringLiteral("Invalid Modbus multiple-write response length");
                return result;
            }
            result.fields.insert(QStringLiteral("startAddress"), int(wordAt(payload, 2)));
            result.fields.insert(QStringLiteral("quantity"), int(wordAt(payload, 4)));
        }
    }

    result.summary = QStringLiteral("Station %1 %2 FC=%3")
                         .arg(station)
                         .arg(role)
                         .arg(functionText(functionCode));
    if (result.fields.contains(QStringLiteral("startAddress"))) {
        result.summary += QStringLiteral(" start=%1 quantity=%2")
                              .arg(result.fields.value(QStringLiteral("startAddress")).toInt())
                              .arg(result.fields.value(QStringLiteral("quantity")).toInt());
    } else if (result.fields.contains(QStringLiteral("address"))) {
        result.summary += QStringLiteral(" address=%1 value=%2")
                              .arg(result.fields.value(QStringLiteral("address")).toInt())
                              .arg(result.fields.value(QStringLiteral("value")).toInt());
    } else if (result.fields.contains(QStringLiteral("byteCount"))) {
        result.summary += QStringLiteral(" bytes=%1")
                              .arg(result.fields.value(QStringLiteral("byteCount")).toInt());
    }
    return result;
}
