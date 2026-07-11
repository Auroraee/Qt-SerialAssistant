#ifndef MODBUSRTUDECODER_H
#define MODBUSRTUDECODER_H

#include "frametypes.h"

class ModbusRtuDecoder
{
public:
    static DecodeResult decode(const QByteArray &frame, FrameDirection direction);
};

#endif // MODBUSRTUDECODER_H
