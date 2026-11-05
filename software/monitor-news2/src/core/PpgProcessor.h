#pragma once

#include "core/Biquad.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

struct PpgSample
{
    uint32_t red = 0;
    uint32_t infrared = 0;
};

// Transforma o PPG bruto do MAX30102 (vermelho + IR a 25 Hz) em FC, SpO₂ e FR.
//
// - Contato: IR acima do limiar com histerese e confirmação, para o dedo encostando ou saindo
//   não oscilar o estado.
// - FC: passa-banda 0,5–4 Hz no IR invertido (o sangue da sístole absorve mais IR); um pico só
//   conta se for o máximo numa janela de ±0,24 s e passar de metade da amplitude recente. A FC é
//   a média dos intervalos recentes que estão a até 25% da mediana.
// - SpO₂: razão das razões (AC/DC do vermelho sobre AC/DC do IR) por batimento, mediana dos
//   últimos batimentos, convertida pela curva empírica do design de referência da Maxim (RD117).
//   Não é calibrada clinicamente.
// - FR: a respiração modula o PPG de três formas — linha de base (RIIV), amplitude do pulso
//   (RIAV) e intervalo entre batimentos (RIFV). As séries de amplitude e intervalo são
//   interpoladas entre batimentos regulares (perdidos/duplicados descartados) a 5 Hz; as três
//   são filtradas a 0,13–0,6 Hz (7,8–36 irpm, acima das ondas de Mayer de ~0,1 Hz). Em janelas
//   de 32 s, os espectros normalizados (Hann) das três são somados e a FR é o pico, publicado
//   só se for um máximo local fora da borda da banda e concentrar a potência — fusão espectral
//   que, no PPG real deste sensor, se mostrou mais estável que a autocorrelação.
class PpgProcessor
{
public:
    static constexpr double SampleRateHz = 25.0;
    static constexpr double RespirationSampleRateHz = 5.0;

    struct Output
    {
        bool hasFingerContactChanged = false;
        // valores já normalizados (0..1) para desenhar; vazios quando não há amostra nova
        std::optional<double> plethysmographyDisplay;
        // a curva respiratória chega em rajadas (a cada batimento): pode vir mais de um valor
        std::vector<double> respirationDisplay;
    };

    PpgProcessor();

    Output addSample(const PpgSample& sample);

    bool hasFingerContact() const { return m_hasFingerContact; }
    std::optional<int> heartRate() const;
    std::optional<int> oxygenSaturation() const;
    std::optional<int> respiratoryRate() const;

private:
    struct FilteredSample
    {
        double infrared = 0.0;
        double red = 0.0;
    };

    // Uma modulação respiratória: sinal a 5 Hz filtrado a 0,13–0,6 Hz e a janela usada na
    // análise espectral.
    struct RespiratoryModulation
    {
        Biquad highPass = Biquad::highPass(0.13, RespirationSampleRateHz);
        Biquad lowPass = Biquad::lowPass(0.6, RespirationSampleRateHz);
        bool isPrimed = false;
        int sampleCount = 0;
        std::deque<double> window;

        double add(double value);
    };

    bool updateFingerContact(uint32_t infrared);
    void resetMeasurements();
    void processCardiacSample(const PpgSample& sample, Output& output);
    void detectBeat();
    void onBeat(double peakPosition, double infraredAmplitude, double redAmplitude);
    void addBeatModulations(double beatSeconds, double amplitude, double interval);
    void processRespirationSample(Output& output);
    void estimateRespiratoryRate();
    bool publishRespiratoryRate(const std::vector<double>& spectrum);
    double respirationDisplayValue(double baseline);

    static std::vector<double> normalizedSpectrum(const std::deque<double>& window);
    static std::optional<double> breathRateFromSpectrum(const std::vector<double>& spectrum);
    static double normalizeForDisplay(double value, std::deque<double>& recent, size_t length);

    bool m_hasFingerContact = false;
    int m_contactCandidateCount = 0;

    int64_t m_sampleIndex = 0;
    Biquad m_infraredHighPass;
    Biquad m_infraredLowPass;
    Biquad m_redHighPass;
    Biquad m_redLowPass;
    double m_infraredDc = 0.0;
    double m_redDc = 0.0;
    std::deque<FilteredSample> m_recentFiltered;
    std::deque<double> m_plethysmographyDisplayWindow;
    double m_amplitude = 0.0;
    std::optional<double> m_lastPeakPosition;

    std::deque<double> m_beatIntervals;
    std::deque<double> m_ratios;
    std::deque<double> m_beatAmplitudes;

    struct RegularBeat
    {
        double seconds = 0.0;
        double amplitude = 0.0;
        double interval = 0.0;
    };
    std::optional<RegularBeat> m_lastRegularBeat;
    double m_beatGridSeconds = 0.0;

    double m_baselineAccumulator = 0.0;
    int m_baselineCount = 0;
    int64_t m_respirationIndex = 0;
    RespiratoryModulation m_baselineModulation;
    RespiratoryModulation m_amplitudeModulation;
    RespiratoryModulation m_intervalModulation;
    // curva de respiração da tela: linha de base, passa-banda de 4ª ordem 0,1–0,5 Hz (duas
    // seções de 2ª ordem em cada lado) e escala por envelope de pico
    Biquad m_respirationDisplayHighPass[2];
    Biquad m_respirationDisplayLowPass[2];
    bool m_isRespirationDisplayPrimed = false;
    double m_respirationDisplayEnvelope = 0.0;
    int64_t m_respirationDisplayCount = 0;
    // FR: espectro fundido com média exponencial entre estimativas e mediana das últimas saídas
    std::vector<double> m_averagedRespirationSpectrum;
    std::deque<double> m_recentRespiratoryRates;
    std::optional<double> m_respiratoryRate;
    int64_t m_respiratoryRateAt = 0;
};
