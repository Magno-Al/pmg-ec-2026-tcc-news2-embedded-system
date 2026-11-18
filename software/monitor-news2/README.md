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
média de 4 na FIFO, ou seja, 100 amostras/s, que vão sem alteração para o cálculo. As amostras
que o chip descarta com a FIFO cheia são contadas (aviso no log e coluna `amostras_perdidas` do
traço).

A escolha do i2c-dev está registrada no estudo comparativo i2c-dev × driver IIO do kernel, em
`../bringup/max30102/estudo-comparativo`. O roteiro para validar a leitura a 100 Hz no
equipamento está em `../bringup/max30102/TESTE_I2CDEV_100HZ.md`.

#### Cálculo de FC, SpO₂ e FR

FC, SpO₂ e FR vêm só do método do `../ppg-pipeline` (MSPTD para os batimentos, Smart Fusion de
Karlen para a FR), pelo `PipelineVitalsEstimator`, sobre as amostras de 100 Hz. A cada 5 s, FC
e SpO₂ saem da janela dos últimos 30 s e a FR da janela de 60 s. Uma janela reprovada pelo
controle de qualidade do pipeline (índice de perfusão fora de 0,4–15%) deixa o parâmetro
ausente. A única diferença do `main.cpp` do pipeline: FC e SpO₂ já saem com 30 s de sinal, e lá
só com o buffer cheio, aos 60 s. FC e FR são corrigidas pela taxa real do sensor, medida depois
de 20 s de leitura (o oscilador do MAX30102 fica ~1% abaixo da taxa nominal).

A comparação de métodos que levou a esse cálculo está em `../../comparativo.md`.

#### Dedo no sensor

O pipeline não detecta o dedo. O app decide pelo IR bruto, no `FingerContactDetector`:

- o contato entra acima de **50.000 contagens** e sai abaixo de **30.000**;
- a mudança precisa se manter por **0,48 s** para entrar e **0,32 s** para sair (48 e 32 amostras
  a 100 Hz).

Esses valores são **parâmetro de engenharia, sem fundamento na literatura**. Foram definidos
antes da mudança para 400 Hz com média de 4 e ainda precisam ser calibrados no dispositivo (passo 5 de `../bringup/max30102/TESTE_I2CDEV_100HZ.md`). Só com o
dedo as amostras vão para o estimador, que recomeça do zero a cada troca de contato.

#### Curva da tela

- **Pletismografia (linhas de FC e SpO₂):** é o IR filtrado pelo passa-banda do pipeline
  (`PPGBandpassFilter`), o mesmo valor que entra na janela do cálculo. O `PlethysmographyDisplay`
  normaliza esse valor **só para a tela**, sem efeito em nenhum cálculo: inverte o sinal, para o
  pulso subir, e o leva a 0,1–0,9 pelo mínimo e pelo máximo dos últimos 2 s. Nos primeiros 2–3 s
  de contato, a curva mostra o transitório do filtro, que parte do zero.
- **Respiração:** não há curva. No equipamento, a linha da FR fica sem onda.
- Sem amostras, o build com hardware deixa a curva vazia. Só o build com mock sintetiza a onda a
  partir da frequência.

#### O que a tela mostra

- Sem dedo, as linhas ficam em "AGUARDANDO DEDO", sem alarme.
- Com o dedo parado, FC e SpO₂ aparecem em ~35 s e a FR em ~65 s ou mais. A primeira janela de
  cada medida carrega o transitório do filtro e costuma ser reprovada; no PPG sintético dos
  testes, a primeira FC sai aos 35 s e a primeira FR aos 65 s.
- Até a primeira janela, e sempre que uma janela é reprovada, o parâmetro fica em "—". Ele vai
  como 0 na interface `VitalSignsSource`, e o último valor não se repete. O dedo saindo zera as
  medidas.
- Tirar o dedo depois de ter havido leitura gera o alarme técnico "sem contato".
- A SpO₂ não tem calibração clínica: é a reta 110 − 25·R do pipeline, provisória.
- A temperatura vem do DS18B20 (1-Wire no GPIO 4, pull-up de 4,7 kΩ), lida a cada 10 s numa
  thread própria (`Ds18b20Thermometer`). Com ela o NEWS2 fecha completo; sem o sensor, a
  temperatura fica "—" e o NEWS2 incompleto.

#### Cadência e tendência

O pipeline recalcula a cada 5 s, mas a fonte publica a cada 1 s o último valor, então cada
estimativa se repete ~5 vezes. O `TrendTracker` continua com as 15 últimas leituras (15 s), que
agora cobrem só ~3 estimativas distintas: a seta de tendência reage sobre ~3 janelas do
pipeline. O "último X às HH:MM" mostra a hora da publicação, não a da janela.

#### Custo do cálculo

Uma janela com FC, SpO₂ e FR a 100 Hz custa no PC 9–11 ms em média e 15–18 ms no máximo
(`pipeline-window-benchmark`, configuração `app`, 10 min de PPG sintético). Para a Pi 3, a
estimativa anterior era de 55–60 ms em média. Ela parte da razão entre a placa e o PC medida para
a FC a 100 Hz: ~19 ms na Pi 3 contra 2,4–2,9 ms no PC hoje, uma razão de ~6,5–8. Com os números
de hoje, essa mesma razão dá ~60–90 ms em média e ~100–150 ms no máximo.

A faixa no PC é variação da medida, e não do código: o governador de frequência do PC é
`powersave` e a carga muda entre as rodadas. Rodando alternados no mesmo núcleo (`taskset`), o
código da tag `ultimo-ppgprocessor` e o atual dão a mesma média, 9,7–10,7 ms, e o mesmo máximo,
~11,5 ms. Os máximos de 15–18 ms vieram de rodadas sem núcleo fixo. Com o máximo de ~11,5 ms,
a mesma razão daria ~75–90 ms na Pi; este README e o roteiro usam a faixa maior, de
~100–150 ms, por segurança.

Isso fica abaixo da folga de ~280 ms da FIFO do chip, mas **passa do critério de 50 ms** do
roteiro de validação e **precisa ser medido na placa**. Se não couber, a alternativa
registrada é FC a 100 Hz num estimador e SpO₂/FR a 25 Hz em outro, o que exige reduzir as
amostras a 25 Hz (média de 4). No PC, ela custa 2,4–2,9 ms mais 2,0–2,5 ms por janela
(configurações `fc-100` e `spo2-fr-25` do benchmark).

#### Pendências abertas

- **FR ausente com frequência.** A FR só vem do Smart Fusion de Karlen, que estimou a FR em 55%
  das janelas na BIDMC (`../ppg-pipeline/README.md`, tabela de resultados). Com a FR ausente, o NEWS2 é calculado sobre o total parcial, e o nível de resposta
  usa só os parâmetros presentes. O preditor, que exige o snapshot completo, deixa de rodar com
  a mesma frequência. O comportamento está mantido; aqui só fica registrado.
- **Limiares do dedo:** calibrar no dispositivo (ver "Dedo no sensor").
- **Custo na Pi 3:** medir (ver "Custo do cálculo").
- **Cálculo na thread principal.** O `PipelineVitalsEstimator` roda na thread principal do Qt, a
  mesma da interface e do toque. A `Max30102VitalSignsSource` é criada em `src/main.cpp` sem
  thread própria, o timer de leitura chama `poll()`, e é dentro dele que cada amostra entra no
  estimador; a janela fecha dentro desse `addSample()`. Com o máximo estimado na Pi
  (~100–150 ms por janela, a cada 5 s), a tela e o toque podem travar por esse tempo a cada 5 s.
  A arquitetura fica como está até a medição na placa (passo 5.5 de
  `../bringup/max30102/TESTE_I2CDEV_100HZ.md`).
- **`PipelineVitalsEstimator::Outputs::HeartRateOnly`:** o app não usa; ele serve só para o
  benchmark medir a alternativa. Sai depois da medição na placa, se o cálculo completo a
  100 Hz couber.
- **Comparação `processor-25` do `../ppg-pipeline/scripts/comparison/run_comparison.sh`:** usa a
  ferramenta `ppg-processor-export`, que não existe no app. No HEAD ela não roda; roda a partir
  da tag `ultimo-ppgprocessor` (ver `../../comparativo.md`).

### Depuração no equipamento

- `MONITOR_NEWS2_TRACE=/tmp/trace.csv /root/monitor-news2 --fullscreen` grava cada amostra de
  100 Hz (`s,ms,red,ir,contato,curva_pleth`) e cada publicação
  (`p,ms,fc,spo2,fr,correcao,amostras_perdidas`, com 0 para o parâmetro ausente).
- `kill -USR1 $(pidof monitor-news2)` salva a tela em `/tmp/monitor-news2-<hora>.png`. O eglfs
  desenha direto no vídeo, então não há outro jeito de tirar print.

## Organização

| Pasta | Conteúdo |
|---|---|
| `src/core` | NEWS2, alarme, sessão, `PipelineVitalsEstimator` (FC, SpO₂, FR pelo ppg-pipeline), `FingerContactDetector` e a curva da tela |
| `src/devices` | interfaces `VitalSignsSource` (MAX30102 + DS18B20) e `AlarmIndicator` (LED/buzzer) |
| `src/prediction` | interface `DeteriorationPredictor` (XGBoost, NEWS2 ≥ 5 em 6 h) |
| `src/mock` | implementações simuladas e os sinais de partida do leito 14-B |
| `src/sensors` | MAX30102 (i2c-dev), `Max30102VitalSignsSource` e `Ds18b20Thermometer` |
| `src/app` | `MonitorController` (view-model exposto ao QML como `Monitor`) e `PressureEntry` |
| `src/ui` | itens C++ de desenho: curva em varredura, tendência, hachura, borda tracejada/dupla |
| `qml` | telas: Monitoramento, Registro de PA/O₂, Evolução, Plantão, ajuda da predição |
| `tests` | Qt Test do NEWS2, do alarme, do número por extenso, do estimador, do detector de dedo, da curva e da FIFO do MAX30102 |

## Plantão

O monitor abre sem plantão: sinais vitais e NEWS2 aparecem ao vivo, mas nada é registrado e
a Evolução fica desabilitada. **Iniciar plantão** (tela Plantão) começa o log de eventos, o
histórico do NEWS2, a evolução e a janela de 6 h da predição. **Finalizar plantão** (segurar
2 s) apaga tudo do paciente — inclusive PA/O₂ registrada — para o próximo começar do zero.

## Para integrar o restante do hardware

MAX30102 e DS18B20 já estão integrados (ver acima). Faltam as implementações reais de
`DeteriorationPredictor` (modelo XGBoost) e `AlarmIndicator` (LED e buzzer por GPIO); os pontos
de troca estão em `src/main.cpp`.
