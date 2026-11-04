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

- Sem dedo, as linhas ficam em "AGUARDANDO DEDO", sem alarme. Com o dedo, as curvas de FC e
  SpO₂ passam a mostrar o PPG real e a de FR a respiração extraída do PPG.
- FC em ~5 s, SpO₂ em ~10 s, FR em ~60 s (32 s de sinal limpo + pico espectral claro). Até
  lá, "—".
- Tirar o dedo depois de ter havido leitura gera o alarme técnico "sem contato".
- SpO₂ usa a curva empírica da Maxim (RD117), sem calibração clínica. A temperatura não é
  lida (DS18B20 ainda não montado), então o NEWS2 fica incompleto e a predição não roda.

## Organização

| Pasta | Conteúdo |
|---|---|
| `src/core` | tabela NEWS2, alarme, tipos da sessão e `PpgProcessor` (FC, SpO₂ e FR do PPG) |
| `src/devices` | interfaces `VitalSignsSource` (MAX30102 + DS18B20) e `AlarmIndicator` (LED/buzzer) |
| `src/prediction` | interface `DeteriorationPredictor` (XGBoost, NEWS2 ≥ 5 em 6 h) |
| `src/mock` | implementações simuladas e os sinais de partida do leito 14-B |
| `src/sensors` | driver I2C do MAX30102 e `Max30102VitalSignsSource` (build com hardware) |
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

O MAX30102 já está integrado (ver acima). Faltam implementações reais de temperatura (DS18B20,
`temperatureMeasured`), `DeteriorationPredictor` e `AlarmIndicator`; os pontos de troca estão
em `src/main.cpp`.
