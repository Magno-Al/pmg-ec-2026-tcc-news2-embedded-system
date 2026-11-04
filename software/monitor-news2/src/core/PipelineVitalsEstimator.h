#pragma once

#include "core/PpgProcessor.h"

#include <PPGBuffer.hpp>
#include <PPGFilter.hpp>

#include <cstddef>
#include <optional>

// FC, SpO₂ e FR pelo método do software/ppg-pipeline (MSPTD, Smart Fusion de Karlen; ver o
// README de lá). Reproduz o laço do main.cpp daquele projeto sobre as amostras que o app já lê
// do MAX30102: filtro por canal, buffer de 60 s e, a cada 5 s, qualidade (índice de perfusão),
// FC e SpO₂ nos últimos 30 s e FR nos 60 s. Cada janela substitui os valores da anterior; uma
// janela reprovada deixa o parâmetro sem leitura, como no método original.
//
// Diferença do main.cpp: lá a primeira saída só vem com o buffer cheio (60 s). Aqui FC e SpO₂
// já saem com 30 s de sinal — a janela que o próprio método usa para eles —, para o monitor não
// ficar um minuto sem números a cada vez que o dedo é recolocado. A FR continua com 60 s.
//
// Contato do dedo e curvas da tela continuam com o PpgProcessor: este estimador só calcula.
class PipelineVitalsEstimator
{
public:
    explicit PipelineVitalsEstimator(int sampleRateHz = int(PpgProcessor::SampleRateHz));

    // recomeça do zero (dedo recolocado): o buffer de 60 s não deve misturar dois contatos
    void reset();
    void addSample(const PpgSample& sample);

    std::optional<int> heartRate() const { return m_heartRate; }
    std::optional<int> oxygenSaturation() const { return m_oxygenSaturation; }
    std::optional<int> respiratoryRate() const { return m_respiratoryRate; }

private:
    void evaluateWindow();
    void evaluateCardiacWindowWhileFilling();

    // o sensor do app entrega a intensidade bruta do fotodetector (README do pipeline, seção 4)
    static constexpr bool IsRawIntensity = true;

    int m_sampleRateHz;
    std::optional<PPGBandpassFilter> m_redFilter;
    std::optional<PPGBandpassFilter> m_infraredFilter;
    PPGBuffer m_buffer;
    size_t m_heartRateWindowSamples = 0;
    std::optional<int> m_heartRate;
    std::optional<int> m_oxygenSaturation;
    std::optional<int> m_respiratoryRate;
};
