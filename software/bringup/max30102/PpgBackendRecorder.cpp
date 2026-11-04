// Gravador para o estudo comparativo dos dois acessos ao MAX30102 (i2c-dev x driver IIO).
// Usa as mesmas classes do monitor-news2, então grava exatamente o que o app recebe: amostras
// de vermelho/IR a 25 Hz, com o instante (CLOCK_MONOTONIC) de cada leitura.
//
// Uso: ppg-backend-recorder <i2cdev|iio> <segundos> > gravacao.csv
// Saída: "leitura_s,amostras_na_leitura,red,ir" por amostra; resumo de custo no stderr.

#include "sensors/Max30102.h"
#include "sensors/Max30102Iio.h"

#include <poll.h>
#include <sys/resource.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{

double monotonicSeconds()
{
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec / 1e9;
}

struct Totals
{
    long sampleCount = 0;
    long readCount = 0;
    long emptyReadCount = 0;
};

void writeSamples(double readAt, const std::vector<PpgSample>& samples, Totals& totals)
{
    ++totals.readCount;
    if(samples.empty())
    {
        ++totals.emptyReadCount;
        return;
    }
    for(const PpgSample& sample : samples)
    {
        std::printf("%.6f,%zu,%u,%u\n", readAt, samples.size(), sample.red, sample.infrared);
    }
    totals.sampleCount += long(samples.size());
}

template<typename Sensor, typename WaitForData>
int record(Sensor& sensor, double durationSeconds, WaitForData waitForData)
{
    if(!sensor.open())
    {
        std::fprintf(stderr, "ERRO: %s\n", qPrintable(sensor.errorString()));
        return 1;
    }
    rusage before;
    getrusage(RUSAGE_SELF, &before);
    const double start = monotonicSeconds();
    Totals totals;
    std::printf("leitura_s,amostras_na_leitura,red,ir\n");
    while(monotonicSeconds() - start < durationSeconds)
    {
        waitForData();
        std::vector<PpgSample> samples;
        if(!sensor.readAvailableSamples(samples))
        {
            std::fprintf(stderr, "ERRO: %s\n", qPrintable(sensor.errorString()));
            return 1;
        }
        writeSamples(monotonicSeconds() - start, samples, totals);
    }
    const double elapsed = monotonicSeconds() - start;
    rusage after;
    getrusage(RUSAGE_SELF, &after);
    sensor.close();

    const auto seconds = [](const timeval& value) { return value.tv_sec + value.tv_usec / 1e6; };
    std::fprintf(stderr,
        "duracao_s=%.3f amostras=%ld taxa_hz=%.3f leituras=%ld leituras_vazias=%ld "
        "cpu_user_s=%.4f cpu_sys_s=%.4f trocas_voluntarias=%ld trocas_involuntarias=%ld\n",
        elapsed, totals.sampleCount, totals.sampleCount / elapsed, totals.readCount,
        totals.emptyReadCount, seconds(after.ru_utime) - seconds(before.ru_utime),
        seconds(after.ru_stime) - seconds(before.ru_stime),
        after.ru_nvcsw - before.ru_nvcsw, after.ru_nivcsw - before.ru_nivcsw);
    return 0;
}

}

int main(int argc, char* argv[])
{
    if(argc != 3)
    {
        std::fprintf(stderr, "uso: %s <i2cdev|iio> <segundos>\n", argv[0]);
        return 2;
    }
    const std::string backend = argv[1];
    const double durationSeconds = std::atof(argv[2]);

    if(backend == "i2cdev")
    {
        // mesmo ritmo do app: FIFO consultada a cada 40 ms
        Max30102 sensor;
        return record(sensor, durationSeconds, [] {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        });
    }
    if(backend == "iio")
    {
        // mesmo modelo do app: dorme até o kernel avisar que há dados
        Max30102Iio sensor;
        return record(sensor, durationSeconds, [&sensor] {
            pollfd descriptor = {sensor.notificationDescriptor(), POLLIN, 0};
            poll(&descriptor, 1, 1000);
        });
    }
    std::fprintf(stderr, "backend desconhecido: %s\n", backend.c_str());
    return 2;
}
