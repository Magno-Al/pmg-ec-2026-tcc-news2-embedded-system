#pragma once

#include <QtGlobal>

struct VitalSigns
{
    int heartRate = 0;
    int oxygenSaturation = 0;
    int respiratoryRate = 0;
    double temperature = 0.0;
};

struct VitalSample
{
    qint64 takenAt = 0;
    VitalSigns vitals;
    bool hasFingerContact = true;
};
