#include "MockScenario.h"

namespace MockScenario
{

VitalSigns initialVitals()
{
    VitalSigns vitals;
    vitals.heartRate = 98;
    vitals.oxygenSaturation = 95;
    vitals.respiratoryRate = 21;
    vitals.temperature = 37.2;
    return vitals;
}

PatientSession createSession()
{
    PatientSession session;
    session.patient.age = 68;
    session.patient.ward = QStringLiteral("Enfermaria Clínica Médica");
    session.patient.bed = QStringLiteral("14-B");
    return session;
}

}
