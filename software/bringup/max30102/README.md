# Bring-up do MAX30102

Programas de bancada usados para validar o sensor PPG antes e durante a integração no app.

| Arquivo | O que faz |
|---|---|
| `Max30102HeartRateTest.cpp` | Lê o sensor pela `/dev/i2c-1` e imprime a FC uma vez por segundo (`--raw` imprime o sinal bruto IR,Red). Independente do app. |
| `PpgBackendRecorder.cpp` | Grava o PPG pelas mesmas classes de acesso do app (`i2cdev` ou `iio`), com o instante de cada leitura e o custo de CPU. Usado no estudo comparativo. |
| `PipelineWindowBenchmark.cpp` | Mede o tempo de cálculo de uma janela do `PipelineVitalsEstimator` (FC a 100 Hz, como no modo `refined`, e FC/SpO₂/FR a 25 Hz) sobre PPG sintético. Não usa o sensor. |
| `estudo-comparativo/` | Protocolo, scripts e resultados da comparação i2c-dev × driver IIO. A configuração do i2c-dev descrita lá (100 Hz, média de 4 → 25 Hz) é a da época do estudo. |
| `TESTE_I2CDEV_100HZ.md` | Roteiro para testar o i2c-dev a 100 Hz no equipamento e depois torná-lo o acesso padrão. |

## Compilar (toolchain do Buildroot, a partir da raiz do repositório)

```sh
BR=buildroot/build
CXX=$BR/host/bin/aarch64-buildroot-linux-gnu-g++
D=software/bringup/max30102

$CXX -std=c++17 -O2 -Wall -Wextra -o $D/max30102-hr-test $D/Max30102HeartRateTest.cpp

export PKG_CONFIG_SYSROOT_DIR=$PWD/$BR/staging PKG_CONFIG_LIBDIR=$PWD/$BR/staging/usr/lib/pkgconfig
$CXX -std=c++17 -O2 -Wall -Wextra -fPIC -Isoftware/monitor-news2/src \
    -o $D/ppg-backend-recorder $D/PpgBackendRecorder.cpp \
    software/monitor-news2/src/sensors/Max30102.cpp software/monitor-news2/src/sensors/Max30102Iio.cpp \
    software/monitor-news2/src/sensors/Max30102Fifo.cpp software/monitor-news2/src/core/PpgDecimator.cpp \
    $($BR/host/bin/pkg-config --cflags --libs Qt6Core)

P=software/ppg-pipeline
$CXX -std=c++17 -O2 -DNDEBUG -Wall -Wextra -Isoftware/monitor-news2/src -Isoftware/monitor-news2/tests \
    -I$P/include -o $D/pipeline-window-benchmark $D/PipelineWindowBenchmark.cpp \
    software/monitor-news2/src/core/PipelineVitalsEstimator.cpp \
    $P/src/PPGBuffer.cpp $P/src/PPGFilter.cpp $P/src/PPGPeaks.cpp $P/src/PPGMsptdBeatDetector.cpp \
    $P/src/PPGHeartRate.cpp $P/src/PPGQuality.cpp $P/src/PPGSpO2.cpp \
    $P/src/PPGRespiratorySignal.cpp $P/src/PPGRespiratoryRate.cpp
```

O `ppg-backend-recorder` grava as amostras de 25 Hz que o app entrega ao `PpgProcessor`. Desde o
i2c-dev a 100 Hz, nos dois acessos elas são a média de 4 amostras de 100 Hz feita no app.

Copie o binário para o equipamento (`scp ... root@<ip>:/root/`). Com o acesso IIO carregado
(`max30102.ko`), o endereço 0x57 fica ocupado: rode `/etc/init.d/S30max30102 stop` antes de
usar o `max30102-hr-test`.
