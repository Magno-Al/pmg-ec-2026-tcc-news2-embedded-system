// Mede o tempo de cálculo de uma janela do PipelineVitalsEstimator, o adaptador do app para o
// software/ppg-pipeline. Uma janela fecha a cada 5 s de sinal, a partir de 30 s; o cálculo roda
// dentro do addSample() que a fecha, na thread principal do app (a mesma do timer de leitura do
// sensor). Sem sensor: o sinal é o PPG sintético dos testes (tests/SyntheticPpg.h).
//
// Configurações medidas:
//   app: FC, SpO₂ e FR a 100 Hz (Max30102::FullRateHz), como o app calcula;
//   fc-100 e spo2-fr-25: as duas partes da alternativa registrada no README do app (FC a
//   100 Hz num estimador e SpO₂/FR a 25 Hz em outro); o custo dela é a soma das duas.
//
// Uso: pipeline-window-benchmark [minutos de sinal, padrão 10]
// Compilar em Release (-O2/-O3 e -DNDEBUG): ver README.md desta pasta.

#include "core/PipelineVitalsEstimator.h"

#include "SyntheticPpg.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <vector>

namespace
{

struct Result
{
    std::vector<double> windowMilliseconds;
    double otherSampleMicroseconds = 0.0;
    int heartRateCount = 0;
};

Result run(int sampleRateHz, PipelineVitalsEstimator::Outputs outputs, int minutes)
{
    PipelineVitalsEstimator estimator(sampleRateHz, outputs);
    SyntheticPpg ppg;
    ppg.sampleRateHz = sampleRateHz;

    // a janela fecha no addSample da amostra n (contada a partir de 1) com n ≥ 30 s e
    // (n − 30 s) múltiplo do passo de 5 s, tanto no enchimento do buffer quanto depois dele
    const long firstWindow = 30L * sampleRateHz;
    const long hop = 5L * sampleRateHz;
    const long sampleCount = long(minutes) * 60L * sampleRateHz;

    Result result;
    double otherTotal = 0.0;
    long otherCount = 0;
    for(long n = 1; n <= sampleCount; ++n)
    {
        const PPGSample sample = ppg.next();
        const auto start = std::chrono::steady_clock::now();
        estimator.addSample(sample);
        const auto end = std::chrono::steady_clock::now();
        const double microseconds =
            std::chrono::duration<double, std::micro>(end - start).count();
        if(n >= firstWindow && (n - firstWindow) % hop == 0)
        {
            result.windowMilliseconds.push_back(microseconds / 1000.0);
            if(estimator.preciseHeartRate())
            {
                ++result.heartRateCount;
            }
        }
        else
        {
            otherTotal += microseconds;
            ++otherCount;
        }
    }
    result.otherSampleMicroseconds = otherCount > 0 ? otherTotal / double(otherCount) : 0.0;
    return result;
}

void report(const char* name, const Result& result)
{
    const std::vector<double>& times = result.windowMilliseconds;
    if(times.empty())
    {
        std::printf("%s: nenhuma janela\n", name);
        return;
    }
    const double mean = std::accumulate(times.begin(), times.end(), 0.0) / double(times.size());
    std::vector<double> sorted = times;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[sorted.size() / 2];
    std::printf(
        "%-11s janelas=%zu  média=%.2f ms  mediana=%.2f ms  mínimo=%.2f ms  máximo=%.2f ms  "
        "(demais amostras: %.2f µs cada; FC em %d janelas)\n",
        name, times.size(), mean, median, sorted.front(), sorted.back(),
        result.otherSampleMicroseconds, result.heartRateCount
    );
}

}

int main(int argc, char** argv)
{
    const int minutes = argc > 1 ? std::max(1, std::atoi(argv[1])) : 10;
    std::printf("PPG sintético de %d min por configuração\n", minutes);
    report("app", run(100, PipelineVitalsEstimator::Outputs::All, minutes));
    report("fc-100", run(100, PipelineVitalsEstimator::Outputs::HeartRateOnly, minutes));
    report("spo2-fr-25", run(25, PipelineVitalsEstimator::Outputs::All, minutes));
    return 0;
}
