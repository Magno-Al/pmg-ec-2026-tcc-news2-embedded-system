#include "MockDeteriorationPredictor.h"

#include <algorithm>

std::optional<int> MockDeteriorationPredictor::predictProbability(const PredictionInput& input)
{
    if(!input.snapshot.isComplete)
    {
        return std::nullopt;
    }
    const VitalSigns& vitals = input.currentVitals;
    const int probability = 8
        + input.snapshot.total * 14
        + std::max(0, vitals.respiratoryRate - 18) * 4
        + (vitals.heartRate > 90 ? 5 : 0);
    return std::clamp(probability, 3, 95);
}
