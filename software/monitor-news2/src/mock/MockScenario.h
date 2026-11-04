#pragma once

#include "core/PatientSession.h"
#include "core/VitalSigns.h"

// Leito simulado: sinais de partida do cenário de demonstração (NEWS2 4 com FR em
// subida). O monitor abre sem plantão; os registros começam quando a enfermagem o inicia.
namespace MockScenario
{

VitalSigns initialVitals();

PatientSession createSession();

}
