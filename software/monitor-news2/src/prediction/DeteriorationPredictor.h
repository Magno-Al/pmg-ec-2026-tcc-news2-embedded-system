#pragma once

#include "core/News2Score.h"
#include "core/VitalSigns.h"

#include <QVector>

#include <optional>

struct PredictionInput
{
    News2::Snapshot snapshot;
    VitalSigns currentVitals;
    QVector<VitalSample> lastSixHours;
    int patientAge = 0;
};

// Fronteira com o modelo preditivo (XGBoost embarcado no TCC II): probabilidade, em %, de o
// NEWS2 atingir 5 ou mais nas próximas 6 horas.
class DeteriorationPredictor
{
public:
    virtual ~DeteriorationPredictor() = default;

    virtual std::optional<int> predictProbability(const PredictionInput& input) = 0;
};
