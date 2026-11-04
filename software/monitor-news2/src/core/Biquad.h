#pragma once

#include <cmath>

// Filtro de segunda ordem (Butterworth, RBJ Audio EQ Cookbook) em forma direta I. Usado em
// cascata passa-alta + passa-baixa para formar os passa-bandas do PPG.
class Biquad
{
public:
    static Biquad highPass(double cutoffHz, double sampleRateHz)
    {
        return design(cutoffHz, sampleRateHz, true);
    }

    static Biquad lowPass(double cutoffHz, double sampleRateHz)
    {
        return design(cutoffHz, sampleRateHz, false);
    }

    double process(double input)
    {
        const double output = m_b0 * input + m_b1 * m_input1 + m_b2 * m_input2
            - m_a1 * m_output1 - m_a2 * m_output2;
        m_input2 = m_input1;
        m_input1 = input;
        m_output2 = m_output1;
        m_output1 = output;
        return output;
    }

    // Parte do regime permanente para a entrada dada: sem isso, o degrau do DC do PPG (~10⁵
    // contagens) domina a saída do passa-alta por vários segundos.
    void settle(double input)
    {
        const double gain = (m_b0 + m_b1 + m_b2) / (1.0 + m_a1 + m_a2);
        m_input1 = m_input2 = input;
        m_output1 = m_output2 = input * gain;
    }

private:
    static Biquad design(double cutoffHz, double sampleRateHz, bool isHighPass)
    {
        const double omega = 2.0 * M_PI * cutoffHz / sampleRateHz;
        const double alpha = std::sin(omega) / (2.0 * M_SQRT1_2);
        const double cosine = std::cos(omega);
        const double a0 = 1.0 + alpha;
        Biquad filter;
        if(isHighPass)
        {
            filter.m_b0 = (1.0 + cosine) / 2.0 / a0;
            filter.m_b1 = -(1.0 + cosine) / a0;
        }
        else
        {
            filter.m_b0 = (1.0 - cosine) / 2.0 / a0;
            filter.m_b1 = (1.0 - cosine) / a0;
        }
        filter.m_b2 = filter.m_b0;
        filter.m_a1 = -2.0 * cosine / a0;
        filter.m_a2 = (1.0 - alpha) / a0;
        return filter;
    }

    double m_b0 = 1.0;
    double m_b1 = 0.0;
    double m_b2 = 0.0;
    double m_a1 = 0.0;
    double m_a2 = 0.0;
    double m_input1 = 0.0;
    double m_input2 = 0.0;
    double m_output1 = 0.0;
    double m_output2 = 0.0;
};
