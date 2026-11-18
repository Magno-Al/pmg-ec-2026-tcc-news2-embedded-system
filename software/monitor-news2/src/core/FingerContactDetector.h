#pragma once

#include <cstdint>

// Dedo no sensor, pela intensidade bruta do IR: limiar com histerese (entra acima de 50.000
// contagens, sai abaixo de 30.000) e confirmação (0,48 s seguidos para entrar, 0,32 s para
// sair), para o dedo encostando ou saindo não oscilar o estado. Não calcula nenhum sinal vital:
// só decide se as amostras vão para o PipelineVitalsEstimator, que não tem detecção de dedo.
//
// Os valores são parâmetro de engenharia, sem fundamento na literatura. Foram definidos antes da
// mudança para 400 Hz com média de 4 na FIFO, e falta calibrá-los no dispositivo (ver o README do
// app).
class FingerContactDetector
{
public:
    static constexpr uint32_t FingerOnThreshold = 50000;
    static constexpr uint32_t FingerOffThreshold = 30000;
    static constexpr double ContactConfirmSeconds = 0.48;
    static constexpr double LossConfirmSeconds = 0.32;

    explicit FingerContactDetector(int sampleRateHz);

    // sem contato e sem candidato (sensor reaberto)
    void reset();
    // true quando a amostra confirma uma mudança de estado
    bool update(uint32_t infrared);

    bool hasFingerContact() const { return m_hasFingerContact; }

private:
    int m_contactConfirmSamples;
    int m_lossConfirmSamples;
    bool m_hasFingerContact = false;
    int m_candidateCount = 0;
};
