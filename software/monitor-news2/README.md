# Monitor NEWS2 — interface de beira de leito

Aplicação Qt 6 (C++ + QML) da tela de 7" (1024 × 600, toque) do monitor do TCC. No PC roda
com dados simulados; no equipamento (imagem Buildroot em `../../buildroot`) lê o MAX30102.
Sensores, modelo preditivo e LED/buzzer estão atrás de interfaces.

## Compilar e rodar

```sh
export PATH=$HOME/Qt/Tools/CMake/bin:$HOME/Qt/Tools/Ninja:$PATH
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build
ctest --test-dir build
./build/monitor-news2                          # janela 1024 × 600
./build/monitor-news2 --deterioration-seconds 5  # FR sobe mais rápido → alarme urgente
./build/monitor-news2 --fullscreen             # como no equipamento
```

`--deterioration-seconds 0` desliga a subida da FR. O LED e o buzzer são escritos no console.

### Com o sensor real (Raspberry Pi)

`-DMONITOR_NEWS2_HARDWARE=ON` troca o mock pela `Max30102VitalSignsSource`, que lê o
MAX30102. O pacote do Buildroot (`buildroot/external/package/monitor-news2`) já liga a opção; no
PC o padrão continua sendo o mock. A forma de acesso ao sensor também é escolhida na compilação,
com `-DMONITOR_NEWS2_SENSOR_BACKEND` (no Buildroot, opção "acesso ao MAX30102" do pacote):

- `i2cdev` (padrão): o app configura o chip e lê a FIFO pela `/dev/i2c-1` a cada 40 ms.
- `iio`: driver do kernel (`max30102.ko`, overlay `i2c-sensor,max30102,int_pin=17`), com
  leitura por evento em `/dev/iio:device0`. Exige o INT do sensor no GPIO 17.

A comparação entre os dois está em `../bringup/max30102/estudo-comparativo`.

O cálculo de FC, SpO₂ e FR também é escolhido na compilação, com
`-DMONITOR_NEWS2_PPG_ALGORITHM` (no Buildroot, opção "cálculo de FC/SpO2/FR" do pacote):

- `hybrid` (padrão): FC e SpO₂ pelo método do `../ppg-pipeline` (MSPTD), FR pelo `PpgProcessor`.
- `pipeline`: os três pelo `../ppg-pipeline` (FR pelo Smart Fusion de Karlen).
- `processor`: os três pelo `PpgProcessor` deste app.
- `refined` (padrão da imagem do Buildroot, exige `iio`): SpO₂ e FR pelo `PpgProcessor`; a FC
  começa pelo `PpgProcessor` (~5 s) e passa a vir do `../ppg-pipeline` sobre as amostras de
  100 Hz do driver IIO, sem a média de 4, assim que houver 30 s de sinal (~35 s). Na BIDMC a FC
  do pipeline erra 0,57 bpm a 125 Hz contra 1,49 bpm a 25 Hz.

Em todos os modos, FC e FR são corrigidas pela taxa real do sensor, medida depois de 20 s de
leitura (o oscilador do MAX30102 fica ~1% abaixo da taxa nominal).

O contato do dedo e as curvas da tela são sempre do `PpgProcessor`. O padrão segue a comparação
na BIDMC (`../ppg-pipeline/scripts/comparison`): o pipeline erra menos a FC, e o `PpgProcessor`
erra menos a FR e não confunde as ondas de Mayer (~6–7 irpm) com respiração no sensor real.

- Sem dedo, as linhas ficam em "AGUARDANDO DEDO", sem alarme. Com o dedo, as curvas de FC e
  SpO₂ passam a mostrar o PPG real e a de FR a respiração extraída do PPG.
- Tempo até a primeira leitura com o dedo parado: no `hybrid` (padrão), FC e SpO₂ em ~35 s e FR
  em ~40–45 s; no `processor`, FC em ~5 s e SpO₂ em ~10 s. Até lá, "—". O dedo saindo zera
  as medidas.
- Tirar o dedo depois de ter havido leitura gera o alarme técnico "sem contato".
- SpO₂ sem calibração clínica: curva empírica da Maxim (RD117) no `processor`, 110 − 25·R no
  `pipeline`/`hybrid`.
- A temperatura vem do DS18B20 (1-Wire no GPIO 4, pull-up de 4,7 kΩ), lida a cada 10 s numa
  thread própria (`Ds18b20Thermometer`). Com ela o NEWS2 fecha completo; sem o sensor, a
  temperatura fica "—" e o NEWS2 incompleto.

### Depuração no equipamento

- `MONITOR_NEWS2_TRACE=/tmp/trace.csv /root/monitor-news2 --fullscreen` grava cada amostra de
  25 Hz (bruto, contato e valores das curvas) e cada publicação de FC/SpO₂/FR, com as fontes.
- `kill -USR1 $(pidof monitor-news2)` salva a tela em `/tmp/monitor-news2-<hora>.png`. O eglfs
  desenha direto no vídeo, então não há outro jeito de tirar print.

## Organização

| Pasta | Conteúdo |
|---|---|
| `src/core` | NEWS2, alarme, sessão, `PpgProcessor` e `PipelineVitalsEstimator` (FC, SpO₂, FR) |
| `src/devices` | interfaces `VitalSignsSource` (MAX30102 + DS18B20) e `AlarmIndicator` (LED/buzzer) |
| `src/prediction` | interface `DeteriorationPredictor` (XGBoost, NEWS2 ≥ 5 em 6 h) |
| `src/mock` | implementações simuladas e os sinais de partida do leito 14-B |
| `src/sensors` | MAX30102 (i2c-dev e IIO), `Max30102VitalSignsSource` e `Ds18b20Thermometer` |
| `src/app` | `MonitorController` (view-model exposto ao QML como `Monitor`) e `PressureEntry` |
| `src/ui` | itens C++ de desenho: curva em varredura, tendência, hachura, borda tracejada/dupla |
| `qml` | telas: Monitoramento, Registro de PA/O₂, Evolução, Plantão, ajuda da predição |
| `tests` | Qt Test do NEWS2, do alarme, do número por extenso e do processamento do PPG |

## Plantão

O monitor abre sem plantão: sinais vitais e NEWS2 aparecem ao vivo, mas nada é registrado e
a Evolução fica desabilitada. **Iniciar plantão** (tela Plantão) começa o log de eventos, o
histórico do NEWS2, a evolução e a janela de 6 h da predição. **Finalizar plantão** (segurar
2 s) apaga tudo do paciente — inclusive PA/O₂ registrada — para o próximo começar do zero.

## Para integrar o restante do hardware

MAX30102 e DS18B20 já estão integrados (ver acima). Faltam as implementações reais de
`DeteriorationPredictor` (modelo XGBoost) e `AlarmIndicator` (LED e buzzer por GPIO); os pontos
de troca estão em `src/main.cpp`.
