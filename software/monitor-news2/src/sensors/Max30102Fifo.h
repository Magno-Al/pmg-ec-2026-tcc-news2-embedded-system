#pragma once

#include <PPG.hpp>

#include <cstdint>

// Lógica da FIFO do MAX30102 que não depende do barramento, separada do Max30102 (i2c-dev) para
// poder ser testada sem o sensor. Referência: datasheet do MAX30102, seção "FIFO".
namespace Max30102Fifo
{

// a FIFO guarda 32 amostras; cada uma tem 3 bytes do vermelho seguidos de 3 do IR (modo SpO₂)
constexpr int Depth = 32;
constexpr int BytesPerSample = 6;
// FIFO_WR_PTR, OVF_COUNTER e FIFO_RD_PTR têm 5 bits úteis
constexpr uint8_t PointerMask = 0x1F;
// ADC de 18 bits: os bits acima do 18º não fazem parte da medida
constexpr uint32_t SampleMask = 0x3FFFF;

// Amostras prontas para leitura, pelos ponteiros de escrita e leitura (com a volta do 31 para o
// 0) e pelo contador de overflow. Ponteiros iguais são a FIFO vazia, a não ser que o contador de
// overflow seja maior que zero: aí ela está cheia, com as 32 amostras disponíveis.
int availableSampleCount(uint8_t writePointer, uint8_t readPointer, uint8_t overflowCounter);

// Um canal a partir dos seus 3 bytes, do mais para o menos significativo, nos 18 bits do ADC.
uint32_t channelValue(const uint8_t* bytes);

// Uma amostra a partir dos 6 bytes da FIFO: vermelho nos 3 primeiros, IR nos 3 seguintes.
PPGSample decodeSample(const uint8_t* bytes);

}
