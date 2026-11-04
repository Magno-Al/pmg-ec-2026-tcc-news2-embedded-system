# Bring-up do MAX30102

Programas de bancada usados para validar o sensor PPG antes e durante a integração no app.

| Arquivo | O que faz |
|---|---|
| `Max30102HeartRateTest.cpp` | Lê o sensor pela `/dev/i2c-1` e imprime a FC uma vez por segundo (`--raw` imprime o sinal bruto IR,Red). Independente do app. |
| `PpgBackendRecorder.cpp` | Grava o PPG pelas mesmas classes de acesso do app (`i2cdev` ou `iio`), com o instante de cada leitura e o custo de CPU. Usado no estudo comparativo. |
| `estudo-comparativo/` | Protocolo, scripts e resultados da comparação i2c-dev × driver IIO. |

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
    $($BR/host/bin/pkg-config --cflags --libs Qt6Core)
```

Copie o binário para o equipamento (`scp ... root@<ip>:/root/`). Com o acesso IIO carregado
(`max30102.ko`), o endereço 0x57 fica ocupado: rode `/etc/init.d/S30max30102 stop` antes de
usar o `max30102-hr-test`.
