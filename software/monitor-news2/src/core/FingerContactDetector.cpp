#include "FingerContactDetector.h"

#include <cmath>

FingerContactDetector::FingerContactDetector(int sampleRateHz)
    : m_contactConfirmSamples(int(std::lround(ContactConfirmSeconds * sampleRateHz)))
    , m_lossConfirmSamples(int(std::lround(LossConfirmSeconds * sampleRateHz)))
{
}

void FingerContactDetector::reset()
{
    m_hasFingerContact = false;
    m_candidateCount = 0;
}

bool FingerContactDetector::update(uint32_t infrared)
{
    const bool looksLikeContact = m_hasFingerContact ? infrared >= FingerOffThreshold
                                                     : infrared > FingerOnThreshold;
    if(looksLikeContact == m_hasFingerContact)
    {
        m_candidateCount = 0;
        return false;
    }
    ++m_candidateCount;
    const int requiredCount = m_hasFingerContact ? m_lossConfirmSamples : m_contactConfirmSamples;
    if(m_candidateCount < requiredCount)
    {
        return false;
    }
    m_hasFingerContact = !m_hasFingerContact;
    m_candidateCount = 0;
    return true;
}
