#pragma once

#include "prediction/DeteriorationPredictor.h"

// Substituto do XGBoost: heurística monotônica no escore e na FR, só para a interface ter
// algo coerente a mostrar até o modelo ser embarcado.
class MockDeteriorationPredictor : public DeteriorationPredictor
{
public:
    std::optional<int> predictProbability(const PredictionInput& input) override;
};
