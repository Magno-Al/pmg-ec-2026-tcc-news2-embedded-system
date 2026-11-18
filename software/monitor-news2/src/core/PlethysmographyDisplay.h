#pragma once

#include <cstddef>
#include <deque>

// Curva de pletismografia da tela, só apresentação: não entra em nenhum cálculo. Recebe o IR já
// filtrado pelo passa-banda do ppg-pipeline (PPGBandpassFilter, o mesmo valor que vai para a
// janela do PipelineVitalsEstimator) e o leva para 0,1..0,9 pelo mínimo e máximo dos últimos
// 2 s, invertido: o sangue da sístole absorve mais IR, e a curva sobe no pulso, como nos
// oxímetros. Sinal plano (sem variação na janela) fica em 0,5.
class PlethysmographyDisplay
{
public:
    static constexpr double WindowSeconds = 2.0;

    explicit PlethysmographyDisplay(int sampleRateHz);

    void reset();
    // valor 0..1 para o LiveSignalBuffer
    double add(double infraredFiltered);

private:
    size_t m_windowSamples;
    std::deque<double> m_recent;
};
