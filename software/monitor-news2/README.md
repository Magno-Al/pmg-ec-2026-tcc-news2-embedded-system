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
PC o padrão continua sendo o mock.

O app configura o chip e lê a FIFO pela `/dev/i2c-1` (i2c-dev, em espaço de usuário) a cada
40 ms; o pino INT do sensor não é usado. Nenhum driver do kernel ocupa o endereço 0x57: a imagem
não declara o sensor no device tree e não tem o módulo `max30102`. O chip fica em 400 Hz com
média de 4 na FIFO, ou seja, 100 amostras/s. O app entrega essas amostras sem alteração ao
ppg-pipeline (FC) e a média de cada 4 (25 Hz) ao `PpgProcessor`. As amostras que o
chip descarta com a FIFO cheia são contadas (aviso no log e coluna `amostras_perdidas` do
traço).

A escolha do i2c-dev está registrada no estudo comparativo i2c-dev × driver IIO do kernel, em
`../bringup/max30102/estudo-comparativo`. O roteiro para validar a leitura a 100 Hz no
equipamento está em `../bringup/max30102/TESTE_I2CDEV_100HZ.md`.

Cada parâmetro vem do método que se saiu melhor na comparação na BIDMC e no sensor real
(`../../comparativo.md`, `../ppg-pipeline/scripts/comparison`):

- **FC**: começa pelo `PpgProcessor` (~5 s) e passa a vir do `../ppg-pipeline` (MSPTD) sobre as
  amostras de 100 Hz do sensor, sem a média de 4, assim que houver 30 s de sinal (~35 s). Na
  BIDMC a FC do pipeline erra 0,57 bpm a 125 Hz contra 1,49 bpm a 25 Hz e 3,21 bpm do
  `PpgProcessor`.
- **SpO₂, FR, contato do dedo e curvas**: `PpgProcessor`, a 25 Hz. Ele erra menos a FR e não
  confunde as ondas de Mayer (~6–7 irpm) com respiração no sensor real.

FC e FR são corrigidas pela taxa real do sensor, medida depois de 20 s de leitura (o oscilador
do MAX30102 fica ~1% abaixo da taxa nominal).

- Sem dedo, as linhas ficam em "AGUARDANDO DEDO", sem alarme. Com o dedo, a curva de
  pletismografia passa a mostrar o PPG real e a de FR a respiração extraída do PPG.
- Tempo até a primeira leitura com o dedo parado: FC em ~5 s (refinada pelo pipeline em ~35 s),
  SpO₂ em ~10 s e FR em ~30–40 s. Até lá, "—". O dedo saindo zera as medidas.
- Tirar o dedo depois de ter havido leitura gera o alarme técnico "sem contato".
- SpO₂ sem calibração clínica: curva empírica da Maxim (RD117).
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
| `src/sensors` | MAX30102 (i2c-dev), `Max30102VitalSignsSource` e `Ds18b20Thermometer` |
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
