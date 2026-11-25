#include "Max30102VitalSignsSource.h"

#include "sensors/Max30102Acquisition.h"

#include <QMetaObject>

Max30102VitalSignsSource::Max30102VitalSignsSource(QObject* parent)
    : VitalSignsSource{parent}
    , m_acquisition(new Max30102Acquisition)
{
    m_acquisitionThread.setObjectName(QStringLiteral("max30102"));
    m_acquisition->moveToThread(&m_acquisitionThread);
    connect(
        &m_acquisitionThread, &QThread::finished,
        m_acquisition, &QObject::deleteLater
    );
    connect(
        m_acquisition, &Max30102Acquisition::pulseOximetryMeasured,
        this, &VitalSignsSource::pulseOximetryMeasured
    );
    connect(
        m_acquisition, &Max30102Acquisition::fingerContactChanged,
        this, &VitalSignsSource::fingerContactChanged
    );
    connect(
        &m_thermometer, &Ds18b20Thermometer::temperatureMeasured,
        this, &VitalSignsSource::temperatureMeasured
    );
}

Max30102VitalSignsSource::~Max30102VitalSignsSource()
{
    // sem start() a thread nunca roda e o finished que apagaria a aquisição não chega
    if(!m_acquisitionThread.isRunning() && !m_acquisitionThread.isFinished())
    {
        delete m_acquisition;
        return;
    }
    m_acquisitionThread.quit();
    m_acquisitionThread.wait();
}

void Max30102VitalSignsSource::start()
{
    if(m_acquisitionThread.isRunning())
    {
        return;
    }
    m_acquisitionThread.start();
    QMetaObject::invokeMethod(m_acquisition, &Max30102Acquisition::start, Qt::QueuedConnection);
    m_thermometer.start();
}
