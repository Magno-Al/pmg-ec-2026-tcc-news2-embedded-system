#include "Max30102Fifo.h"

namespace Max30102Fifo
{

int availableSampleCount(uint8_t writePointer, uint8_t readPointer, uint8_t overflowCounter)
{
    if((overflowCounter & PointerMask) > 0)
    {
        return Depth;
    }
    return ((writePointer & PointerMask) - (readPointer & PointerMask)) & (Depth - 1);
}

uint32_t channelValue(const uint8_t* bytes)
{
    const uint32_t value = (uint32_t(bytes[0]) << 16) | (uint32_t(bytes[1]) << 8) | bytes[2];
    return value & SampleMask;
}

PpgSample decodeSample(const uint8_t* bytes)
{
    PpgSample sample;
    sample.red = channelValue(bytes);
    sample.infrared = channelValue(bytes + 3);
    return sample;
}

}
