#ifndef FRAMEASSEMBLER_H
#define FRAMEASSEMBLER_H

#include "frametypes.h"

class FrameAssembler
{
public:
    explicit FrameAssembler(const FrameConfig &config = FrameConfig());

    void setConfig(const FrameConfig &config);
    FrameConfig config() const;

    FrameAssemblyResult append(const QByteArray &data);
    FrameAssemblyResult flush();
    void clear();
    QByteArray bufferedData() const;

private:
    QString validateConfig() const;

    FrameConfig m_config;
    QByteArray m_buffer;
};

#endif // FRAMEASSEMBLER_H
