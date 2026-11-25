# Bring-up do MAX30102

Programas de bancada usados para validar o sensor PPG antes e durante a integração no app.

| Arquivo | O que faz |
|---|---|
| `Max30102HeartRateTest.cpp` | Lê o sensor pela `/dev/i2c-1` e imprime a FC uma vez por segundo (`--raw` imprime o sinal bruto IR,Red). Independente do app. |
| `PipelineWindowBenchmark.cpp` | Mede o tempo de cálculo de uma janela do `PipelineVitalsEstimator` (FC a 100 Hz, como no app, e FC/SpO₂/FR a 25 Hz) sobre PPG sintético. Não usa o sensor. |
| `estudo-comparativo/` | Protocolo, scripts e resultados da comparação i2c-dev × driver IIO do kernel, que levou à escolha do i2c-dev. Os scripts rodam sobre o commit `16b4a99` (ver o README do estudo). |
| `TESTE_I2CDEV_100HZ.md` | Roteiro para validar no equipamento a leitura do sensor a 100 Hz pelo i2c-dev. |

## Compilar (toolchain do Buildroot, a partir da raiz do repositório)

```sh
BR=buildroot/build
CXX=$BR/host/bin/aarch64-buildroot-linux-gnu-g++
D=software/bringup/max30102

$CXX -std=c++17 -O2 -Wall -Wextra -o $D/max30102-hr-test $D/Max30102HeartRateTest.cpp

P=software/ppg-pipeline
$CXX -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Isoftware/monitor-news2/src -Isoftware/monitor-news2/tests \
    -I$P/include -o $D/pipeline-window-benchmark $D/PipelineWindowBenchmark.cpp \
    software/monitor-news2/src/core/PipelineVitalsEstimator.cpp \
    $P/src/PPGBuffer.cpp $P/src/PPGFilter.cpp $P/src/PPGPeaks.cpp $P/src/PPGMsptdBeatDetector.cpp \
    $P/src/PPGHeartRate.cpp $P/src/PPGQuality.cpp $P/src/PPGSpO2.cpp \
    $P/src/PPGRespiratorySignal.cpp $P/src/PPGRespiratoryRate.cpp
```

Copie o binário para o equipamento (`scp ... root@<ip>:/root/`). O `max30102-hr-test` e o app
configuram o mesmo chip pela `/dev/i2c-1`: feche o app antes de usá-lo.
