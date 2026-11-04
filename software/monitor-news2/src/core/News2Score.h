#pragma once

#include <QtGlobal>

#include <array>
#include <optional>

// Tabela NEWS2 do Royal College of Physicians (2017). O nível de consciência fica fora do
// cálculo nesta fase do projeto: o escore usa os seis parâmetros abaixo.
namespace News2
{

enum class Parameter
{
    RespiratoryRate,
    OxygenSaturation,
    SupplementalOxygen,
    SystolicPressure,
    HeartRate,
    Temperature
};

constexpr int ParameterCount = 6;

constexpr std::array<Parameter, ParameterCount> AllParameters = {
    Parameter::RespiratoryRate,
    Parameter::OxygenSaturation,
    Parameter::SupplementalOxygen,
    Parameter::SystolicPressure,
    Parameter::HeartRate,
    Parameter::Temperature
};

enum class ResponseLevel
{
    None = 0,
    SingleParameter = 1,
    Urgent = 2,
    Emergency = 3
};

int scoreRespiratoryRate(int breathsPerMinute);
int scoreOxygenSaturationScale1(int saturation);
int scoreOxygenSaturationScale2(int saturation, bool isOnSupplementalOxygen);
int scoreSupplementalOxygen(bool isOnSupplementalOxygen);
int scoreSystolicPressure(int millimetersOfMercury);
int scoreHeartRate(int beatsPerMinute);
int scoreTemperature(double celsius);

struct Inputs
{
    std::optional<int> respiratoryRate;
    std::optional<int> oxygenSaturation;
    std::optional<bool> isOnSupplementalOxygen;
    std::optional<int> systolicPressure;
    std::optional<int> heartRate;
    std::optional<double> temperature;
    bool useOxygenScale2 = false;
};

struct Snapshot
{
    std::array<std::optional<int>, ParameterCount> subscores{};
    int total = 0;
    bool isComplete = false;
    std::optional<Parameter> parameterScoringThree;
    qint64 takenAt = 0;

    std::optional<int> subscore(Parameter parameter) const;
};

Snapshot evaluate(const Inputs& inputs, qint64 takenAt);

ResponseLevel responseLevel(const Snapshot& snapshot);

// Intervalo de aferição prescrito pelo protocolo; 0 significa monitoramento contínuo.
int monitoringIntervalMinutes(const Snapshot& snapshot);

}
