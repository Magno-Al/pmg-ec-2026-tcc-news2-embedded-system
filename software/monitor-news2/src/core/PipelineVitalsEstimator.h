#pragma once

#include <PPG.hpp>
#include <PPGBuffer.hpp>
#include <PPGFilter.hpp>

#include <cstddef>
#include <optional>

// FC, SpO₂ e FR pelo método do software/ppg-pipeline (MSPTD, Smart Fusion de Karlen; ver o
// README de lá). Reproduz o laço do main.cpp daquele projeto sobre as amostras de 100 Hz que o
// app lê do MAX30102: filtro por canal, buffer de 60 s e, a cada 5 s, qualidade (índice de
// perfusão), FC e SpO₂ nos últimos 30 s e FR nos 60 s. Cada janela substitui os valores da
// anterior; uma janela reprovada deixa o parâmetro sem leitura, como no método original, sem
// repetir o valor anterior.
//
// Diferença do main.cpp: lá a primeira saída só vem com o buffer cheio (60 s). Aqui FC e SpO₂
// já saem com 30 s de sinal — a janela que o próprio método usa para eles —, para o monitor não
// ficar um minuto sem números a cada vez que o dedo é recolocado. A FR continua com 60 s.
//
// O estimador não detecta o dedo: quem chama só entrega amostras com o dedo no sensor
// (FingerContactDetector) e o zera a cada troca de contato.
class PipelineVitalsEstimator
{
public:
    enum class Outputs
    {
        All,
        // só FC: não usado pelo app; serve para medir a alternativa de FC a 100 Hz com SpO₂ e FR
        // a 25 Hz (pipeline-window-benchmark, em software/bringup/max30102). Pendência: sai
        // depois da medição na placa, se o cálculo completo a 100 Hz couber (README do app)
        HeartRateOnly
    };

    explicit PipelineVitalsEstimator(int sampleRateHz, Outputs outputs = Outputs::All);

    // recomeça do zero (dedo recolocado): o buffer de 60 s não deve misturar dois contatos
    void reset();
    // devolve o IR filtrado pelo passa-banda do pipeline, o mesmo valor que entra na janela
    // (a curva da tela usa esse valor; ver PlethysmographyDisplay)
    double addSample(const PPGSample& sample);

    std::optional<int> heartRate() const;
    // FC sem arredondar, para quem ainda vai corrigir pela taxa real do sensor
    std::optional<double> preciseHeartRate() const { return m_heartRate; }
    std::optional<int> oxygenSaturation() const { return m_oxygenSaturation; }
    std::optional<int> respiratoryRate() const { return m_respiratoryRate; }

private:
    void evaluateWindow();
    void evaluateCardiacWindowWhileFilling();

    // o sensor do app entrega a intensidade bruta do fotodetector (README do pipeline, seção 4)
    static constexpr bool IsRawIntensity = true;

    int m_sampleRateHz;
    Outputs m_outputs;
    std::optional<PPGBandpassFilter> m_redFilter;
    std::optional<PPGBandpassFilter> m_infraredFilter;
    PPGBuffer m_buffer;
    size_t m_heartRateWindowSamples = 0;
    std::optional<double> m_heartRate;
    std::optional<int> m_oxygenSaturation;
    std::optional<int> m_respiratoryRate;
};
