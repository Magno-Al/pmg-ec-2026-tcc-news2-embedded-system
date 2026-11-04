#include "PipelineVitalsEstimator.h"

#include <PPG.hpp>
#include <PPGHeartRate.hpp>
#include <PPGQuality.hpp>
#include <PPGRespiratoryRate.hpp>
#include <PPGSpO2.hpp>

#include <cmath>

namespace
{

std::optional<int> rounded(std::optional<double> value)
{
    if(!value)
    {
        return std::nullopt;
    }
    return int(std::lround(*value));
}

}

PipelineVitalsEstimator::PipelineVitalsEstimator(int sampleRateHz)
    : m_sampleRateHz(sampleRateHz)
{
    reset();
}

void PipelineVitalsEstimator::reset()
{
    m_redFilter.emplace(PPG_FILTER_LOW_CUTOFF_HZ, PPG_FILTER_HIGH_CUTOFF_HZ, m_sampleRateHz);
    m_infraredFilter.emplace(PPG_FILTER_LOW_CUTOFF_HZ, PPG_FILTER_HIGH_CUTOFF_HZ, m_sampleRateHz);
    m_buffer = make_ppg_buffer(m_sampleRateHz);
    m_heartRateWindowSamples = seconds_to_samples(PPG_WINDOW_SECONDS_HR_SPO2, m_sampleRateHz);
    m_heartRate.reset();
    m_oxygenSaturation.reset();
    m_respiratoryRate.reset();
}

void PipelineVitalsEstimator::addSample(const PpgSample& sample)
{
    const PPGWindowSample windowSample{
        sample.red,
        sample.infrared,
        m_redFilter->process(sample.red),
        m_infraredFilter->process(sample.infrared)
    };
    const bool wasFull = ppg_buffer_is_full(m_buffer);
    if(ppg_buffer_push(m_buffer, windowSample))
    {
        evaluateWindow();
    }
    else if(!wasFull)
    {
        evaluateCardiacWindowWhileFilling();
    }
}

void PipelineVitalsEstimator::evaluateCardiacWindowWhileFilling()
{
    // mesma cadência do buffer cheio: com 30 s de sinal e depois a cada 5 s, até os 60 s
    const size_t filled = m_buffer.count;
    if(filled < m_heartRateWindowSamples
       || (filled - m_heartRateWindowSamples) % m_buffer.hop_samples != 0)
    {
        return;
    }
    // last_window() lê o fim do vetor, que só faz sentido com o buffer cheio; durante o
    // enchimento a janela são as últimas amostras recebidas
    const PPGWindowView heartRateWindow{
        m_buffer.samples.data() + (filled - m_heartRateWindowSamples),
        m_heartRateWindowSamples
    };
    const PPGQualityResult quality =
        assess_window_quality(heartRateWindow, heartRateWindow, IsRawIntensity);

    m_heartRate.reset();
    if(quality.usable_for_hr)
    {
        m_heartRate = rounded(calculate_heart_rate(heartRateWindow, m_sampleRateHz).bpm);
    }
    m_oxygenSaturation.reset();
    if(quality.usable_for_spo2)
    {
        m_oxygenSaturation = rounded(calculate_spo2(heartRateWindow));
    }
}

void PipelineVitalsEstimator::evaluateWindow()
{
    const PPGWindowView heartRateWindow = last_window(m_buffer, m_heartRateWindowSamples);
    const PPGWindowView respiratoryWindow = last_window(m_buffer, m_buffer.buffer_capacity);
    const PPGQualityResult quality =
        assess_window_quality(heartRateWindow, respiratoryWindow, IsRawIntensity);

    m_heartRate.reset();
    if(quality.usable_for_hr)
    {
        m_heartRate = rounded(calculate_heart_rate(heartRateWindow, m_sampleRateHz).bpm);
    }

    m_oxygenSaturation.reset();
    if(quality.usable_for_spo2)
    {
        m_oxygenSaturation = rounded(calculate_spo2(heartRateWindow));
    }

    m_respiratoryRate.reset();
    if(quality.usable_for_rr)
    {
        m_respiratoryRate = rounded(
            calculate_respiratory_rate(respiratoryWindow, m_sampleRateHz).fused_rpm
        );
    }
}
