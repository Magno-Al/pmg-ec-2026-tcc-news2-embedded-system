#include "News2Score.h"

#include <cmath>

namespace News2
{

int scoreRespiratoryRate(int breathsPerMinute)
{
    if(breathsPerMinute <= 8)
    {
        return 3;
    }
    if(breathsPerMinute <= 11)
    {
        return 1;
    }
    if(breathsPerMinute <= 20)
    {
        return 0;
    }
    if(breathsPerMinute <= 24)
    {
        return 2;
    }
    return 3;
}

int scoreOxygenSaturationScale1(int saturation)
{
    if(saturation <= 91)
    {
        return 3;
    }
    if(saturation <= 93)
    {
        return 2;
    }
    if(saturation <= 95)
    {
        return 1;
    }
    return 0;
}

int scoreOxygenSaturationScale2(int saturation, bool isOnSupplementalOxygen)
{
    if(saturation <= 83)
    {
        return 3;
    }
    if(saturation <= 85)
    {
        return 2;
    }
    if(saturation <= 87)
    {
        return 1;
    }
    if(saturation <= 92 || !isOnSupplementalOxygen)
    {
        return 0;
    }
    if(saturation <= 94)
    {
        return 1;
    }
    if(saturation <= 96)
    {
        return 2;
    }
    return 3;
}

int scoreSupplementalOxygen(bool isOnSupplementalOxygen)
{
    return isOnSupplementalOxygen ? 2 : 0;
}

int scoreSystolicPressure(int millimetersOfMercury)
{
    if(millimetersOfMercury <= 90)
    {
        return 3;
    }
    if(millimetersOfMercury <= 100)
    {
        return 2;
    }
    if(millimetersOfMercury <= 110)
    {
        return 1;
    }
    if(millimetersOfMercury <= 219)
    {
        return 0;
    }
    return 3;
}

int scoreHeartRate(int beatsPerMinute)
{
    if(beatsPerMinute <= 40)
    {
        return 3;
    }
    if(beatsPerMinute <= 50)
    {
        return 1;
    }
    if(beatsPerMinute <= 90)
    {
        return 0;
    }
    if(beatsPerMinute <= 110)
    {
        return 1;
    }
    if(beatsPerMinute <= 130)
    {
        return 2;
    }
    return 3;
}

int scoreTemperature(double celsius)
{
    const long tenths = std::lround(celsius * 10.0);
    if(tenths <= 350)
    {
        return 3;
    }
    if(tenths <= 360)
    {
        return 1;
    }
    if(tenths <= 380)
    {
        return 0;
    }
    if(tenths <= 390)
    {
        return 1;
    }
    return 2;
}

std::optional<int> Snapshot::subscore(Parameter parameter) const
{
    return subscores[static_cast<int>(parameter)];
}

Snapshot evaluate(const Inputs& inputs, qint64 takenAt)
{
    Snapshot snapshot;
    snapshot.takenAt = takenAt;

    auto setSubscore = [&snapshot](Parameter parameter, std::optional<int> value) {
        snapshot.subscores[static_cast<int>(parameter)] = value;
    };

    const bool isOnOxygen = inputs.isOnSupplementalOxygen.value_or(false);

    if(inputs.respiratoryRate)
    {
        setSubscore(Parameter::RespiratoryRate, scoreRespiratoryRate(*inputs.respiratoryRate));
    }
    if(inputs.oxygenSaturation)
    {
        const int saturation = *inputs.oxygenSaturation;
        setSubscore(
            Parameter::OxygenSaturation,
            inputs.useOxygenScale2 ? scoreOxygenSaturationScale2(saturation, isOnOxygen)
                                   : scoreOxygenSaturationScale1(saturation)
        );
    }
    if(inputs.isOnSupplementalOxygen)
    {
        setSubscore(Parameter::SupplementalOxygen, scoreSupplementalOxygen(isOnOxygen));
    }
    if(inputs.systolicPressure)
    {
        setSubscore(Parameter::SystolicPressure, scoreSystolicPressure(*inputs.systolicPressure));
    }
    if(inputs.heartRate)
    {
        setSubscore(Parameter::HeartRate, scoreHeartRate(*inputs.heartRate));
    }
    if(inputs.temperature)
    {
        setSubscore(Parameter::Temperature, scoreTemperature(*inputs.temperature));
    }

    int availableCount = 0;
    for(Parameter parameter : AllParameters)
    {
        const std::optional<int> value = snapshot.subscore(parameter);
        if(!value)
        {
            continue;
        }
        ++availableCount;
        snapshot.total += *value;
        if(*value == 3 && !snapshot.parameterScoringThree)
        {
            snapshot.parameterScoringThree = parameter;
        }
    }
    snapshot.isComplete = availableCount == ParameterCount;
    return snapshot;
}

ResponseLevel responseLevel(const Snapshot& snapshot)
{
    if(snapshot.total >= 7)
    {
        return ResponseLevel::Emergency;
    }
    if(snapshot.total >= 5)
    {
        return ResponseLevel::Urgent;
    }
    if(snapshot.parameterScoringThree)
    {
        return ResponseLevel::SingleParameter;
    }
    return ResponseLevel::None;
}

int monitoringIntervalMinutes(const Snapshot& snapshot)
{
    if(snapshot.total >= 7)
    {
        return 0;
    }
    if(snapshot.total >= 5 || snapshot.parameterScoringThree)
    {
        return 60;
    }
    if(snapshot.total >= 1)
    {
        return 240;
    }
    return 720;
}

}
