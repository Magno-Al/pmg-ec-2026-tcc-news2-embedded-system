# Bring-up do MAX30102

Programas de bancada usados para validar o sensor PPG antes e durante a integração no app.

| Arquivo | O que faz |
|---|---|
| `PipelineWindowBenchmark.cpp` | Mede o tempo de cálculo de uma janela do `PipelineVitalsEstimator` sobre PPG sintético: `app` é FC/SpO₂/FR a 100 Hz, como o app calcula; `fc-100` e `spo2-fr-25` são as duas partes da alternativa registrada no README do app. Não usa o sensor. |
| `estudo-comparativo/` | Protocolo, scripts e resultados da comparação i2c-dev × driver IIO do kernel, que levou à escolha do i2c-dev. Os scripts rodam sobre o commit `16b4a99` (ver o README do estudo). |
| `TESTE_I2CDEV_100HZ.md` | Roteiro para validar no equipamento a leitura do sensor a 100 Hz pelo i2c-dev. |

## Compilar (toolchain do Buildroot, a partir da raiz do repositório)

```sh
BR=buildroot/build
CXX=$BR/host/bin/aarch64-buildroot-linux-gnu-g++
D=software/bringup/max30102
P=software/ppg-pipeline
$CXX -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Isoftware/monitor-news2/src -Isoftware/monitor-news2/tests \
    -I$P/include -o $D/pipeline-window-benchmark $D/PipelineWindowBenchmark.cpp \
    software/monitor-news2/src/core/PipelineVitalsEstimator.cpp \
    $P/src/PPGBuffer.cpp $P/src/PPGFilter.cpp $P/src/PPGPeaks.cpp $P/src/PPGMsptdBeatDetector.cpp \
    $P/src/PPGHeartRate.cpp $P/src/PPGQuality.cpp $P/src/PPGSpO2.cpp \
    $P/src/PPGRespiratorySignal.cpp $P/src/PPGRespiratoryRate.cpp
```

Copie o binário para o equipamento (`scp ... root@<ip>:/root/`).
