#include "PpgProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{

// IR com o dedo encostado fica na casa de 10⁵ contagens; sem dedo, abaixo de 10³. A
// histerese evita que o dedo apoiado de leve fique alternando o estado.
constexpr uint32_t FingerOnThreshold = 50000;
constexpr uint32_t FingerOffThreshold = 30000;
constexpr int ContactConfirmSamples = 12;
constexpr int LossConfirmSamples = 8;

// O dedo assentando no sensor gera um transitório que inflaria o limiar de picos: a amplitude
// só é aprendida depois de 3 s de contato.
constexpr int64_t WarmupSamples = 75;
constexpr int PeakHalfWindow = 6;
constexpr double AmplitudeDecayPerSample = 0.995;
constexpr double PeakThresholdFraction = 0.5;
constexpr double MinimumBeatIntervalSeconds = 0.3;
constexpr double MaximumBeatIntervalSeconds = 2.0;
constexpr size_t RecentFilteredCapacity = 80;
constexpr double DcSmoothing = 0.02;
// batimento perdido ou contado em dobro: intervalo fora de ±30% da mediana recente
constexpr double RegularBeatTolerance = 0.3;

constexpr size_t BeatIntervalHistory = 8;
constexpr size_t MinimumIntervalsForRate = 3;
constexpr double IntervalTolerance = 0.25;
constexpr size_t RatioHistory = 8;
constexpr size_t MinimumRatiosForSaturation = 4;
constexpr double MinimumRatio = 0.2;
constexpr double MaximumRatio = 2.0;

constexpr int DecimationFactor = 5;
// As modulações só começam com o sinal já no platô (fim do aquecimento cardíaco, 3 s após o
// contato): o degrau de 50k contagens do dedo encostando leva ~10 s para sair do passa-alta.
// Depois disso, mais 5 s descartados e 30 s de janela.
constexpr int RespirationWarmupSamples = 25;
constexpr size_t RespirationWindowSamples = 160;
constexpr int64_t RespirationEstimateEverySamples = 25;
constexpr double BeatModulationStepSeconds = 1.0 / PpgProcessor::RespirationSampleRateHz;
// 0,13–0,6 Hz = 7,8–36 irpm. Abaixo disso o PPG tem as ondas de Mayer (~0,1 Hz,
// vasomotoras), que no registro real competiam com a respiração.
constexpr double MinimumBreathHz = 0.13;
constexpr double MaximumBreathHz = 0.6;
constexpr double SpectrumStepHz = 0.005;
// o pico tem que concentrar ao menos 25% da potência da banda a ±1,5 irpm dele
constexpr double BreathPeakHalfWidthHz = 0.025;
constexpr double MinimumBreathPeakPower = 0.25;
constexpr double MinimumBeatsPerBreath = 4.0;
constexpr size_t BeatAmplitudeHistory = 8;
// batimento com amplitude fora de 0,5–2× a mediana (dedo mexendo, primeiro pulso do contato)
constexpr double AmplitudeOutlierFactor = 2.0;
constexpr int64_t RespiratoryRateHoldSamples = 100;

constexpr size_t PlethysmographyDisplaySamples = 50;
constexpr size_t RespirationDisplaySamples = 75;

double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}

}

double PpgProcessor::RespiratoryModulation::add(double value)
{
    if(!isPrimed)
    {
        highPass.settle(value);
        isPrimed = true;
    }
    const double filtered = lowPass.process(highPass.process(value));
    ++sampleCount;
    if(sampleCount > RespirationWarmupSamples)
    {
        window.push_back(filtered);
        if(window.size() > RespirationWindowSamples)
        {
            window.pop_front();
        }
    }
    return filtered;
}

PpgProcessor::PpgProcessor()
{
    resetMeasurements();
}

PpgProcessor::Output PpgProcessor::addSample(const PpgSample& sample)
{
    Output output;
    if(updateFingerContact(sample.infrared))
    {
        output.hasFingerContactChanged = true;
        resetMeasurements();
    }
    if(!m_hasFingerContact)
    {
        return output;
    }

    processCardiacSample(sample, output);
    m_baselineAccumulator += sample.infrared;
    ++m_baselineCount;
    if(m_baselineCount == DecimationFactor)
    {
        processRespirationSample();
    }
    return output;
}

std::optional<int> PpgProcessor::heartRate() const
{
    if(m_beatIntervals.size() < MinimumIntervalsForRate)
    {
        return std::nullopt;
    }
    const double typicalInterval = median({m_beatIntervals.begin(), m_beatIntervals.end()});
    double sum = 0.0;
    int count = 0;
    for(double interval : m_beatIntervals)
    {
        if(std::fabs(interval - typicalInterval) <= IntervalTolerance * typicalInterval)
        {
            sum += interval;
            ++count;
        }
    }
    if(count < 2)
    {
        return std::nullopt;
    }
    return int(std::lround(60.0 / (sum / count)));
}

std::optional<int> PpgProcessor::oxygenSaturation() const
{
    if(m_ratios.size() < MinimumRatiosForSaturation)
    {
        return std::nullopt;
    }
    const double ratio = median({m_ratios.begin(), m_ratios.end()});
    const double saturation = -45.060 * ratio * ratio + 30.354 * ratio + 94.845;
    return int(std::lround(std::clamp(saturation, 70.0, 100.0)));
}

std::optional<int> PpgProcessor::respiratoryRate() const
{
    if(!m_respiratoryRate)
    {
        return std::nullopt;
    }
    return int(std::lround(*m_respiratoryRate));
}

bool PpgProcessor::updateFingerContact(uint32_t infrared)
{
    const bool looksLikeContact = m_hasFingerContact ? infrared >= FingerOffThreshold
                                                     : infrared > FingerOnThreshold;
    if(looksLikeContact == m_hasFingerContact)
    {
        m_contactCandidateCount = 0;
        return false;
    }
    ++m_contactCandidateCount;
    const int requiredCount = m_hasFingerContact ? LossConfirmSamples : ContactConfirmSamples;
    if(m_contactCandidateCount < requiredCount)
    {
        return false;
    }
    m_hasFingerContact = !m_hasFingerContact;
    m_contactCandidateCount = 0;
    return true;
}

void PpgProcessor::resetMeasurements()
{
    m_sampleIndex = 0;
    m_infraredHighPass = Biquad::highPass(0.5, SampleRateHz);
    m_infraredLowPass = Biquad::lowPass(4.0, SampleRateHz);
    m_redHighPass = Biquad::highPass(0.5, SampleRateHz);
    m_redLowPass = Biquad::lowPass(4.0, SampleRateHz);
    m_infraredDc = 0.0;
    m_redDc = 0.0;
    m_recentFiltered.clear();
    m_plethysmographyDisplayWindow.clear();
    m_amplitude = 0.0;
    m_lastPeakPosition.reset();

    m_beatIntervals.clear();
    m_ratios.clear();
    m_beatAmplitudes.clear();
    m_lastRegularBeat.reset();
    m_beatGridSeconds = 0.0;

    m_baselineAccumulator = 0.0;
    m_baselineCount = 0;
    m_respirationIndex = 0;
    m_baselineModulation = RespiratoryModulation();
    m_amplitudeModulation = RespiratoryModulation();
    m_intervalModulation = RespiratoryModulation();
    m_respirationDisplayWindow.clear();
    m_respiratoryRate.reset();
    m_respiratoryRateAt = 0;
}

void PpgProcessor::processCardiacSample(const PpgSample& sample, Output& output)
{
    const double infrared = sample.infrared;
    const double red = sample.red;
    if(m_sampleIndex == 0)
    {
        m_infraredHighPass.settle(-infrared);
        m_redHighPass.settle(-red);
        m_infraredDc = infrared;
        m_redDc = red;
    }
    ++m_sampleIndex;
    m_infraredDc += DcSmoothing * (infrared - m_infraredDc);
    m_redDc += DcSmoothing * (red - m_redDc);

    const FilteredSample filtered{
        m_infraredLowPass.process(m_infraredHighPass.process(-infrared)),
        m_redLowPass.process(m_redHighPass.process(-red))
    };
    m_recentFiltered.push_back(filtered);
    if(m_recentFiltered.size() > RecentFilteredCapacity)
    {
        m_recentFiltered.pop_front();
    }
    output.plethysmographyDisplay = normalizeForDisplay(
        filtered.infrared, m_plethysmographyDisplayWindow, PlethysmographyDisplaySamples
    );

    if(m_sampleIndex <= WarmupSamples)
    {
        return;
    }
    m_amplitude = std::max(m_amplitude * AmplitudeDecayPerSample, std::fabs(filtered.infrared));
    detectBeat(output);
}

void PpgProcessor::detectBeat(Output& output)
{
    const size_t windowLength = 2 * PeakHalfWindow + 1;
    const size_t size = m_recentFiltered.size();
    if(size < windowLength)
    {
        return;
    }
    const size_t center = size - 1 - PeakHalfWindow;
    const double candidate = m_recentFiltered[center].infrared;
    for(size_t index = size - windowLength; index < size; ++index)
    {
        if(index != center && m_recentFiltered[index].infrared >= candidate)
        {
            return;
        }
    }
    if(candidate < PeakThresholdFraction * m_amplitude)
    {
        return;
    }

    // vértice da parábola pelos vizinhos: resolução abaixo dos 40 ms de uma amostra
    const double before = m_recentFiltered[center - 1].infrared;
    const double after = m_recentFiltered[center + 1].infrared;
    const double curvature = before - 2.0 * candidate + after;
    const double offset = curvature != 0.0 ? 0.5 * (before - after) / curvature : 0.0;
    const int64_t centerIndex = m_sampleIndex - 1 - PeakHalfWindow;
    const double peakPosition = double(centerIndex) + offset;

    if(!m_lastPeakPosition)
    {
        m_lastPeakPosition = peakPosition;
        return;
    }
    const double intervalSeconds = (peakPosition - *m_lastPeakPosition) / SampleRateHz;
    if(intervalSeconds < MinimumBeatIntervalSeconds)
    {
        return;
    }
    if(intervalSeconds > MaximumBeatIntervalSeconds)
    {
        m_lastPeakPosition = peakPosition;
        return;
    }

    const int64_t oldestIndex = m_sampleIndex - int64_t(size);
    const int64_t beatStart = std::max<int64_t>(
        oldestIndex, int64_t(std::ceil(*m_lastPeakPosition))
    );
    double infraredLow = candidate;
    double infraredHigh = candidate;
    double redLow = m_recentFiltered[center].red;
    double redHigh = redLow;
    for(int64_t index = beatStart; index <= centerIndex; ++index)
    {
        const FilteredSample& value = m_recentFiltered[size_t(index - oldestIndex)];
        infraredLow = std::min(infraredLow, value.infrared);
        infraredHigh = std::max(infraredHigh, value.infrared);
        redLow = std::min(redLow, value.red);
        redHigh = std::max(redHigh, value.red);
    }
    onBeat(peakPosition, infraredHigh - infraredLow, redHigh - redLow, output);
}

void PpgProcessor::onBeat(
    double peakPosition,
    double infraredAmplitude,
    double redAmplitude,
    Output& output
)
{
    const double intervalSeconds = (peakPosition - *m_lastPeakPosition) / SampleRateHz;
    m_lastPeakPosition = peakPosition;

    const bool hasHistory = m_beatIntervals.size() >= MinimumIntervalsForRate;
    const double typicalInterval = hasHistory
        ? median({m_beatIntervals.begin(), m_beatIntervals.end()}) : intervalSeconds;
    m_beatIntervals.push_back(intervalSeconds);
    if(m_beatIntervals.size() > BeatIntervalHistory)
    {
        m_beatIntervals.pop_front();
    }

    // Um batimento perdido vira um intervalo dobrado; o primeiro pulso do contato, uma
    // amplitude de 3×. Nas séries de amplitude e intervalo isso é um degrau que domina o
    // espectro da respiração, então só batimentos regulares entram nelas e na SpO₂.
    const double relativeAmplitude = infraredAmplitude / m_infraredDc;
    const bool hasRegularInterval = std::fabs(intervalSeconds - typicalInterval)
        <= RegularBeatTolerance * typicalInterval;
    bool hasRegularAmplitude = true;
    if(m_beatAmplitudes.size() >= MinimumIntervalsForRate)
    {
        const double typicalAmplitude = median({m_beatAmplitudes.begin(), m_beatAmplitudes.end()});
        hasRegularAmplitude = relativeAmplitude <= AmplitudeOutlierFactor * typicalAmplitude
            && relativeAmplitude >= typicalAmplitude / AmplitudeOutlierFactor;
    }
    m_beatAmplitudes.push_back(relativeAmplitude);
    if(m_beatAmplitudes.size() > BeatAmplitudeHistory)
    {
        m_beatAmplitudes.pop_front();
    }
    if(!hasRegularInterval || !hasRegularAmplitude)
    {
        return;
    }

    if(infraredAmplitude > 0.0 && redAmplitude > 0.0 && m_infraredDc > 0.0 && m_redDc > 0.0)
    {
        const double ratio = (redAmplitude / m_redDc) / (infraredAmplitude / m_infraredDc);
        if(ratio >= MinimumRatio && ratio <= MaximumRatio)
        {
            m_ratios.push_back(ratio);
            if(m_ratios.size() > RatioHistory)
            {
                m_ratios.pop_front();
            }
        }
    }

    addBeatModulations(peakPosition / SampleRateHz, relativeAmplitude, intervalSeconds, output);
}

void PpgProcessor::addBeatModulations(
    double beatSeconds,
    double amplitude,
    double interval,
    Output& output
)
{
    if(!m_lastRegularBeat)
    {
        m_lastRegularBeat = RegularBeat{beatSeconds, amplitude, interval};
        m_beatGridSeconds = beatSeconds;
        return;
    }
    // Reamostra a 5 Hz por interpolação linear entre o batimento regular anterior e este:
    // a série chega com um batimento de atraso, irrelevante para uma janela de 32 s.
    const RegularBeat previous = *m_lastRegularBeat;
    const double span = beatSeconds - previous.seconds;
    while(m_beatGridSeconds <= beatSeconds)
    {
        const double fraction = span > 0.0 ? (m_beatGridSeconds - previous.seconds) / span : 1.0;
        const double amplitudeFiltered = m_amplitudeModulation.add(
            previous.amplitude + (amplitude - previous.amplitude) * fraction
        );
        m_intervalModulation.add(previous.interval + (interval - previous.interval) * fraction);
        // a modulação de amplitude é a que mostrou o padrão respiratório mais limpo no
        // sensor real: é ela que a curva de respiração desenha
        output.respirationDisplay.push_back(normalizeForDisplay(
            amplitudeFiltered, m_respirationDisplayWindow, RespirationDisplaySamples
        ));
        m_beatGridSeconds += BeatModulationStepSeconds;
    }
    m_lastRegularBeat = RegularBeat{beatSeconds, amplitude, interval};
}

void PpgProcessor::processRespirationSample()
{
    const double baseline = m_baselineAccumulator / m_baselineCount;
    m_baselineAccumulator = 0.0;
    m_baselineCount = 0;
    ++m_respirationIndex;

    if(m_sampleIndex <= WarmupSamples)
    {
        return;
    }
    m_baselineModulation.add(-baseline);
    if(m_respirationIndex % RespirationEstimateEverySamples == 0)
    {
        estimateRespiratoryRate();
    }
}

void PpgProcessor::estimateRespiratoryRate()
{
    const auto expireHeldRate = [this]() {
        if(m_respiratoryRate
           && m_respirationIndex - m_respiratoryRateAt > RespiratoryRateHoldSamples)
        {
            m_respiratoryRate.reset();
        }
    };
    if(m_baselineModulation.window.size() < RespirationWindowSamples)
    {
        expireHeldRate();
        return;
    }
    const std::vector<double> baselineSpectrum = normalizedSpectrum(m_baselineModulation.window);

    // Amplitude e intervalo são amostrados uma vez por batimento: com menos de ~4 batimentos
    // por respiração o espectro deles ganha um pico falso (subharmônica). Nessa faixa vale só
    // a linha de base, amostrada sempre.
    const std::optional<double> baselineRate = breathRateFromSpectrum(baselineSpectrum);
    const std::optional<int> currentHeartRate = heartRate();
    const bool hasFewBeatsPerBreath = baselineRate && currentHeartRate
        && *currentHeartRate / *baselineRate < MinimumBeatsPerBreath;
    if(hasFewBeatsPerBreath)
    {
        m_respiratoryRate = baselineRate;
        m_respiratoryRateAt = m_respirationIndex;
        return;
    }

    std::vector<double> fusedSpectrum = baselineSpectrum;
    int spectrumCount = 1;
    for(const RespiratoryModulation* modulation : {&m_amplitudeModulation, &m_intervalModulation})
    {
        if(modulation->window.size() < RespirationWindowSamples)
        {
            continue;
        }
        const std::vector<double> spectrum = normalizedSpectrum(modulation->window);
        for(size_t index = 0; index < spectrum.size(); ++index)
        {
            fusedSpectrum[index] += spectrum[index];
        }
        ++spectrumCount;
    }
    if(spectrumCount >= 2)
    {
        for(double& power : fusedSpectrum)
        {
            power /= spectrumCount;
        }
        if(const auto rate = breathRateFromSpectrum(fusedSpectrum))
        {
            m_respiratoryRate = rate;
            m_respiratoryRateAt = m_respirationIndex;
            return;
        }
    }
    expireHeldRate();
}

std::vector<double> PpgProcessor::normalizedSpectrum(const std::deque<double>& window)
{
    // tira a reta de mínimos quadrados (resto de deriva) e aplica Hann antes da DFT
    const size_t length = window.size();
    const double center = (length - 1) / 2.0;
    double mean = 0.0;
    for(double value : window)
    {
        mean += value;
    }
    mean /= length;
    double slopeNumerator = 0.0;
    double slopeDenominator = 0.0;
    for(size_t index = 0; index < length; ++index)
    {
        slopeNumerator += (index - center) * (window[index] - mean);
        slopeDenominator += (index - center) * (index - center);
    }
    const double slope = slopeNumerator / slopeDenominator;
    std::vector<double> prepared(length);
    for(size_t index = 0; index < length; ++index)
    {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * M_PI * index / (length - 1));
        prepared[index] = (window[index] - mean - slope * (index - center)) * hann;
    }

    const int binCount =
        int(std::lround((MaximumBreathHz - MinimumBreathHz) / SpectrumStepHz)) + 1;
    std::vector<double> spectrum(binCount);
    double total = 0.0;
    for(int bin = 0; bin < binCount; ++bin)
    {
        const double omega = 2.0 * M_PI * (MinimumBreathHz + bin * SpectrumStepHz)
            / RespirationSampleRateHz;
        double real = 0.0;
        double imaginary = 0.0;
        for(size_t index = 0; index < length; ++index)
        {
            real += prepared[index] * std::cos(omega * index);
            imaginary -= prepared[index] * std::sin(omega * index);
        }
        spectrum[bin] = real * real + imaginary * imaginary;
        total += spectrum[bin];
    }
    if(total > 0.0)
    {
        for(double& power : spectrum)
        {
            power /= total;
        }
    }
    return spectrum;
}

std::optional<double> PpgProcessor::breathRateFromSpectrum(const std::vector<double>& spectrum)
{
    const auto peak = std::max_element(spectrum.begin(), spectrum.end());
    const size_t bin = size_t(peak - spectrum.begin());
    // pico na borda é a encosta de algo fora da banda (deriva, ondas de Mayer), não respiração
    if(bin == 0 || bin + 1 >= spectrum.size())
    {
        return std::nullopt;
    }
    const int halfWidthBins = int(std::lround(BreathPeakHalfWidthHz / SpectrumStepHz));
    double peakPower = 0.0;
    for(int offset = -halfWidthBins; offset <= halfWidthBins; ++offset)
    {
        const int neighbor = int(bin) + offset;
        if(neighbor >= 0 && neighbor < int(spectrum.size()))
        {
            peakPower += spectrum[neighbor];
        }
    }
    if(peakPower < MinimumBreathPeakPower)
    {
        return std::nullopt;
    }
    const double before = spectrum[bin - 1];
    const double after = spectrum[bin + 1];
    const double curvature = before - 2.0 * *peak + after;
    const double offset = curvature != 0.0 ? 0.5 * (before - after) / curvature : 0.0;
    return 60.0 * (MinimumBreathHz + (bin + offset) * SpectrumStepHz);
}

double PpgProcessor::normalizeForDisplay(double value, std::deque<double>& recent, size_t length)
{
    recent.push_back(value);
    if(recent.size() > length)
    {
        recent.pop_front();
    }
    const auto [lowest, highest] = std::minmax_element(recent.begin(), recent.end());
    const double range = *highest - *lowest;
    if(range <= 1e-9)
    {
        return 0.5;
    }
    return 0.1 + 0.8 * (value - *lowest) / range;
}
