#pragma once

#include "core/PpgProcessor.h"

#include <cmath>
#include <cstdint>
#include <random>

// PPG sintético com parâmetros conhecidos. A respiração modula o sinal das três formas que o
// processador procura: linha de base (RIIV), amplitude do pulso (RIAV) e FC (arritmia sinusal
// respiratória, RIFV). A razão R entre as perfusões do vermelho e do IR define a SpO₂.
class SyntheticPpg
{
public:
    double heartRate = 72.0;
    double respiratoryRate = 15.0;
    double ratio = 0.6;
    double noiseCounts = 30.0;
    double sampleRateHz = PpgProcessor::SampleRateHz;

    PpgSample next()
    {
        const double seconds = m_sampleIndex / sampleRateHz;
        ++m_sampleIndex;
        const double breath = std::sin(2.0 * M_PI * respiratoryRate / 60.0 * seconds);
        const double instantRate = heartRate * (1.0 + 0.04 * breath);
        m_cardiacPhase = std::fmod(m_cardiacPhase + instantRate / 60.0
                                   / sampleRateHz, 1.0);

        const double infraredDc = 120000.0 * (1.0 + 0.004 * breath);
        const double infraredPerfusion = 0.01 * (1.0 + 0.15 * breath);
        const double redDc = 140000.0 * (1.0 + 0.004 * breath);
        const double redPerfusion = ratio * infraredPerfusion;
        const double pulse = pulseShape(m_cardiacPhase);

        PpgSample sample;
        sample.infrared = uint32_t(infraredDc * (1.0 - infraredPerfusion * pulse) + noise());
        sample.red = uint32_t(redDc * (1.0 - redPerfusion * pulse) + noise());
        return sample;
    }

private:
    // subida rápida na sístole, descida lenta com a incisura dicrótica
    static double pulseShape(double phase)
    {
        const double main = phase < 0.15 ? phase / 0.15 : std::exp(-(phase - 0.15) * 4.0);
        const double notch = 0.2 * std::exp(-std::pow((phase - 0.45) / 0.05, 2));
        return main + notch;
    }

    double noise()
    {
        return std::normal_distribution<double>(0.0, noiseCounts)(m_generator);
    }

    int64_t m_sampleIndex = 0;
    double m_cardiacPhase = 0.0;
    std::mt19937 m_generator{7};
};
