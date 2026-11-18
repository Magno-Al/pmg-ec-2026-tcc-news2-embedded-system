# Validação no equipamento da leitura do MAX30102 a 100 Hz por i2c-dev

O app lê o MAX30102 pela `/dev/i2c-1` (i2c-dev, `software/monitor-news2/src/sensors/Max30102.cpp`).
Ele configura o chip a 400 Hz internos, com média de 4 na FIFO, ou seja, 100 amostras/s. As
amostras de 100 Hz vão sem alteração para o ppg-pipeline (`PipelineVitalsEstimator`), que
calcula FC, SpO₂ e FR. O `FingerContactDetector` decide se há dedo pelo IR bruto; só com o dedo
as amostras entram no cálculo. O app também conta as amostras que o chip descarta quando a FIFO
enche (contador de overflow).

A imagem não declara o sensor no device tree (sem `dtoverlay=i2c-sensor,max30102` no
`config.txt`) e não tem o módulo `max30102` (`# CONFIG_MAX30102 is not set` no
`linux-news2.fragment`). Assim, nenhum driver do kernel ocupa o endereço 0x57, e o pino INT não é
usado. A escolha do i2c-dev está registrada em `estudo-comparativo/`.

Até agora, isso foi verificado só com testes automatizados, sem o sensor. Este roteiro valida no
equipamento.

**Custo do cálculo: ponto de atenção.** O app calcula FC, SpO₂ e FR a 100 Hz. A estimativa para
a Pi 3 é de 55–60 ms por janela em média. Com a medição de hoje no PC, ela vai a ~60–90 ms em
média e ~100–150 ms no máximo (ver o README do app, "Custo do cálculo"). Isso **passa do
critério de 50 ms** do passo 4 e precisa ser medido na placa. Se não couber, a alternativa
registrada é FC a 100 Hz num estimador e SpO₂/FR a 25 Hz em outro. O cálculo roda na thread
principal do Qt, a mesma da tela e do toque: enquanto uma janela é calculada, a cada 5 s, a tela
e o toque ficam parados (passo 5.5).

**Ponto principal a observar.** A FIFO do chip guarda 32 amostras, ou seja, 320 ms a 100 Hz. É o
único buffer entre o chip e o app, que a lê a cada 40 ms na thread principal. Se a thread
principal ficar travada por mais de cerca de 280 ms, amostras se perdem.

## 0. Antes de ir para a bancada

- [ ] Confirmar no datasheet do MAX30102 (tabela de configurações permitidas no modo SpO₂, taxa
      × largura de pulso) que **400 amostras/s com pulso de 411 µs** é uma combinação aceita.
      O driver max30102 do Linux usa a mesma combinação, mas ela não foi conferida na tabela.
- [ ] Confirmar no datasheet o comportamento da FIFO cheia com `FIFO_ROLLOVER_EN = 1`. O código
      assume que, com overflow, os ponteiros de escrita e leitura ficam iguais e
      `OVF_COUNTER > 0`, e aí lê as 32 amostras. Ver `Max30102Fifo::availableSampleCount`.
- [ ] No PC, com o Qt 6.5 ou mais novo: `ctest --test-dir build` passa, incluindo
      `tst_max30102fifo`, `tst_pipelinevitalsestimator`, `tst_fingercontactdetector` e
      `tst_plethysmographydisplay`.

## 1. Gerar e gravar a imagem

O `config.txt` e o fragmento do kernel fazem parte da imagem, então é preciso **regerar a imagem
inteira** com o Buildroot, não só o pacote do app. O `config.txt` vai para a partição de boot
pelo `rpi-firmware`.

```sh
cd buildroot
make
make flash SD=/dev/sdX
```

O app fica em `/root/monitor-news2`. Compile também a ferramenta de medição de tempo e copie-a
para o equipamento. O comando está no `README.md` desta pasta (`pipeline-window-benchmark`).

```sh
scp software/bringup/max30102/pipeline-window-benchmark root@<ip>:/root/
```

## 2. Conferir que nenhum driver ocupa o sensor

Logo depois do boot, antes de abrir o app:

- [ ] `/dev/i2c-1` existe: `ls -l /dev/i2c-1`.
- [ ] `i2cdetect -y 1` mostra `57` na posição 0x57, e não `UU`. `UU` indicaria um driver do kernel
      ligado ao endereço.
- [ ] Nem `/sys/bus/i2c/devices/1-0057` existe: `ls /sys/bus/i2c/devices/`. Sem o overlay, o
      kernel não cria o dispositivo, e portanto não há `1-0057/driver`.
- [ ] `lsmod | grep max30102` não mostra nada.
- [ ] `modinfo max30102` falha. O módulo não deve existir na imagem.
- [ ] O app lê o sensor a 100 Hz: passo 3 e critérios do passo 4.

## 3. Medir durante pelo menos 5 minutos

Com o voluntário em repouso, o dedo parado no sensor desde o início e um oxímetro comercial no
outro dedo como referência de FC:

```sh
MONITOR_NEWS2_TRACE=/tmp/i2cdev.csv /root/monitor-news2 --fullscreen 2>&1 | tee /tmp/i2cdev.log
```

Enquanto o app roda, em outro terminal, confirme a configuração do chip. O registrador 0x0A deve
valer `0x2f`.

```sh
i2cget -f -y 1 0x57 0x0a
```

Encerre o app depois de pelo menos 5 minutos de contato. Depois, com o app fechado, meça o tempo
de cálculo das janelas:

```sh
/root/pipeline-window-benchmark 10
```

A linha `app` dá a média e o máximo por janela do cálculo que o app faz (FC, SpO₂ e FR a
100 Hz). As linhas `fc-100` e `spo2-fr-25` são as duas partes da alternativa; o custo dela é a
soma. Repita a medição com o app aberto, para ver o efeito da concorrência com a interface.

### O que tirar do traço

Copie `/tmp/i2cdev.csv` e `/tmp/i2cdev.log` para o PC. As linhas do traço são:

- `s,ms,red,ir,contato,curva_pleth`: cada amostra de 100 Hz lida do chip, com ou sem dedo.
  `curva_pleth` é o valor da curva da tela, vazio sem dedo;
- `p,ms,fc,spo2,fr,correcao,amostras_perdidas`: cada publicação, uma vez por segundo e só com o
  dedo. O valor 0 é o parâmetro ausente: antes da primeira janela ou com a janela reprovada.

| Medida | Como obter |
|---|---|
| Taxa efetiva (100 Hz) | `100 × correcao` na última linha `p`. A correção é a taxa medida dividida por 100 e vale a partir de 20 s; o fator é limitado a ±5%. Confira também pela contagem de linhas `s` dividida pela duração. |
| Amostras perdidas | Última coluna das linhas `p`. Avisos `FIFO do chip transbordou` no log. |
| FC, SpO₂ e FR exibidas | Colunas `fc`, `spo2` e `fr`. |
| Tempo até a 1ª FC, SpO₂ e FR | `ms` da primeira linha `p` com o valor > 0, menos o `ms` da primeira linha `s` com `contato = 1` daquele contato. |
| Tempo de cálculo por janela | Saída do `pipeline-window-benchmark` (média e máximo da linha `app`). |
| Falhas de leitura | Linhas `MAX30102: ... reconectando` no log. |

Resumo automático, no PC. Ele trata cada contato do dedo separadamente, o que serve também para
o teste do dedo fora e de volta (passo 5):

```python
import csv, statistics, sys

samples, publications = [], []
for row in csv.reader(open(sys.argv[1])):
    if row and row[0] == "s":
        samples.append(row)
    elif row and row[0] == "p":
        publications.append(row)

duration_s = (int(samples[-1][1]) - int(samples[0][1])) / 1000
print(f"duração: {duration_s:.0f} s, amostras de 100 Hz: {len(samples)}")
print(f"taxa pela contagem: {len(samples) / duration_s:.2f} Hz")
print(f"taxa pela correção: {100 * float(publications[-1][5]):.2f} Hz")
print(f"amostras perdidas: {publications[-1][6]}")

# contatos: trechos seguidos de linhas s com contato = 1
episodes, start = [], None
for previous, row in zip([None] + samples, samples):
    touching = row[4] == "1"
    if touching and start is None:
        start = int(row[1])
    elif not touching and start is not None:
        episodes.append((start, int(previous[1])))
        start = None
if start is not None:
    episodes.append((start, int(samples[-1][1])))

for number, (begin, end) in enumerate(episodes, 1):
    rows = [r for r in publications if begin <= int(r[1]) <= end + 1000]
    print(f"\ncontato {number}: {(end - begin) / 1000:.1f} s")
    for column, name in ((2, "FC"), (3, "SpO2"), (4, "FR")):
        present = [r for r in rows if int(r[column]) > 0]
        if not present:
            print(f"  {name}: nenhuma leitura")
            continue
        first_ms = int(present[0][1])
        after = [r for r in rows if int(r[1]) >= first_ms]
        values = [int(r[column]) for r in present]
        absent = sum(1 for r in after if int(r[column]) == 0)
        print(f"  {name}: 1ª em {(first_ms - begin) / 1000:.1f} s; "
              f"média {statistics.mean(values):.1f}, desvio {statistics.pstdev(values):.1f}, "
              f"mín {min(values)}, máx {max(values)}; "
              f"ausente em {absent} de {len(after)} s depois da 1ª")
        if name == "FC":
            jumps = [abs(a - b) for a, b in zip(values, values[1:])]
            print(f"  maior salto da FC entre segundos: {max(jumps, default=0)} bpm")
```

Salve como `resumo_traco.py` e rode `python3 resumo_traco.py i2cdev.csv`.

## 4. Critérios de aprovação

Todos devem valer numa medição de pelo menos 5 minutos com o dedo parado. De preferência, em
duas rodadas.

- [ ] Registrador 0x0A = `0x2f` durante a medição.
- [ ] Taxa efetiva entre **98 e 101 Hz**, pela correção e pela contagem.
- [ ] **Nenhuma amostra perdida**: `amostras_perdidas = 0` e nenhum aviso de transbordo no log.
- [ ] Nenhuma reconexão (`reconectando`) no log.
- [ ] Primeira FC e primeira SpO₂ em até **40 s** após o contato. O esperado é de 30 a 35 s: a
      janela de 30 s costuma ser reprovada pelo transitório do filtro, e a seguinte sai em 35 s.
- [ ] Primeira FR em até **70 s** após o contato. O esperado é de 60 a 65 s, pelo mesmo motivo,
      com a janela de 60 s. Se o Smart Fusion rejeitar as primeiras janelas, a FR demora mais:
      registre o tempo e a fração de segundos com FR ausente, para comparar com os 55% de
      cobertura da BIDMC.
- [ ] FC estável depois da primeira FC:
  - desvio padrão de até 3 bpm em repouso;
  - nenhum salto maior que 10 bpm entre segundos consecutivos;
  - nenhum segundo sem FC com o dedo parado. Cada janela reprovada deixa a FC em "—" por ~5 s.
    Registre quantos segundos ficaram assim.
- [ ] FC média a até 2 bpm da do oxímetro de referência.
- [ ] Tempo de cálculo por janela na Raspberry, com máximo bem abaixo da folga da FIFO, por
      exemplo **< 50 ms**. A folga é de cerca de 280 ms: 320 ms da FIFO menos os 40 ms entre
      leituras. A estimativa para a linha `app` passa desse valor (ver o início do roteiro): se
      o máximo medido passar de 50 ms, registre o valor e as linhas `fc-100` e `spo2-fr-25`,
      que dão o custo da alternativa.

## 5. Validação do cálculo só pelo ppg-pipeline

Itens do cálculo de FC, SpO₂ e FR pelo ppg-pipeline. Use o mesmo comando de traço do passo 3,
uma gravação por item.

### 5.1 Tempo até a primeira FC, SpO₂ e FR

- [ ] Com o dedo parado desde o início, a tela mostra "—" em FC, SpO₂ e FR até a primeira
      janela. Nenhum valor aparece antes disso.
- [ ] FC e SpO₂ aparecem em 30–40 s e a FR em 60–70 s (pelo `resumo_traco.py`), o que é
      compatível com as janelas de 30 s e 60 s do pipeline.
- [ ] A linha de FR não mostra onda; as de FC e SpO₂ mostram o PPG real, subindo no pulso.

### 5.2 Dedo fora e de volta

Depois de pelo menos 70 s de contato, com FC, SpO₂ e FR na tela:

- [ ] Tire o dedo. Em menos de 1 s, as linhas mudam para "SEM SINAL", os valores para "—" e o
      alarme técnico "sem contato" aparece. As linhas `p` do traço param.
- [ ] Recoloque o dedo depois de ~10 s. A primeira linha `p` do novo contato tem
      `fc = spo2 = fr = 0`. Nenhum valor do contato anterior reaparece.
- [ ] FC e SpO₂ voltam em 30–40 s e a FR em 60–70 s a partir do novo contato (bloco
      "contato 2" do `resumo_traco.py`).
- [ ] Repita com o dedo saindo e voltando rápido, em menos de 1 s, e com o dedo apoiado de
      leve. Registre se o contato oscila, o que aparece como vários contatos curtos no resumo.

### 5.3 IR bruto com e sem dedo, para os limiares do detector

Os limiares do `FingerContactDetector` são parâmetro de engenharia: entra acima de 50.000
contagens e sai abaixo de 30.000, com confirmação de 0,48 s e 0,32 s. Foram definidos antes da
mudança para 400 Hz com média de 4, e é preciso conferir que separam bem "com dedo" de "sem
dedo" nesta configuração do chip.

- [ ] Grave 30 s sem nada no sensor, 30 s com o dedo parado, 30 s com o dedo apoiado de leve,
      30 s com o dedo pressionando e 30 s sem nada de novo. Anote os horários de cada trecho.
- [ ] Grave também 30 s sem dedo com a luz da sala acesa sobre o sensor.
- [ ] Para cada trecho, tire os percentis 1, 50 e 99 do IR bruto das linhas `s`:

```python
import csv, statistics, sys

rows = [r for r in csv.reader(open(sys.argv[1])) if r and r[0] == "s"]
start = int(rows[0][1])
for block in range(0, (int(rows[-1][1]) - start) // 10000 + 1):
    ir = sorted(int(r[3]) for r in rows if (int(r[1]) - start) // 10000 == block)
    if ir:
        q = statistics.quantiles(ir, n=100)
        print(f"{block * 10:4d}–{block * 10 + 10:4d} s: IR p1 {q[0]:.0f}, "
              f"p50 {q[49]:.0f}, p99 {q[98]:.0f}")
```

- [ ] Critério: o p99 sem dedo, inclusive com luz, fica bem abaixo de 30.000, e o p1 com o dedo,
      inclusive apoiado de leve, fica bem acima de 50.000. Se não ficar, registre os valores
      para recalibrar os limiares antes de usar o equipamento.

### 5.4 CPU do app com o cálculo a 100 Hz

- [ ] Com o app aberto, o dedo no sensor e FC, SpO₂ e FR na tela, meça o CPU do app em 60 s:

```sh
PID=$(pidof monitor-news2)
A=$(awk '{print $14 + $15}' /proc/$PID/stat); sleep 60
B=$(awk '{print $14 + $15}' /proc/$PID/stat)
awk -v a=$A -v b=$B 'BEGIN { printf "CPU do app: %.1f%% de um núcleo\n", (b - a) / 60 }'
```

  O cálculo usa `USER_HZ = 100`, o padrão do kernel do Pi: (B − A) tiques / 100 / 60 s × 100%.
- [ ] Repita sem o dedo, com a tela em "AGUARDANDO DEDO". A diferença é o custo do cálculo.
- [ ] Compare a diferença com o esperado pelo `pipeline-window-benchmark` rodado na placa:
      média da linha `app` ÷ 5 s. Por exemplo, 60 ms por janela dão ~1,2% de um núcleo. Uma
      diferença muito maior indica custo fora do cálculo das janelas, como o traço ou a curva.
- [ ] Durante a medição, nenhuma amostra perdida e nenhum aviso de transbordo no log.

### 5.5 Tempo de resposta da tela durante o cálculo de uma janela

O `PipelineVitalsEstimator` roda na thread principal do Qt. A `Max30102VitalSignsSource` é
criada em `src/main.cpp` sem thread própria, e o timer de leitura chama `poll()`, que entrega as
amostras ao estimador. Enquanto uma janela é calculada, a cada 5 s com o dedo no sensor, a thread
principal não desenha a curva nem atende o toque.

- [ ] **Bloqueio da thread principal, pelo traço.** O `ms` de cada linha `s` é gravado logo
      depois de a amostra entrar no estimador. Entre amostras do mesmo lote, a diferença é de
      ~0 ms; entre lotes, de ~40 ms. A amostra que fecha uma janela chega atrasada pelo tempo do
      cálculo. Com uma gravação de pelo menos 2 min com o dedo parado:

```python
import csv, statistics, sys

rows = [r for r in csv.reader(open(sys.argv[1])) if r and r[0] == "s"]
gaps = [(int(b[1]) - int(a[1]), int(b[1]))
        for a, b in zip(rows, rows[1:]) if a[4] == "1" and b[4] == "1"]
late = [(gap, at) for gap, at in gaps if gap > 45]
print(f"intervalos acima de 45 ms com dedo: {len(late)}")
for gap, at in sorted(late, reverse=True)[:10]:
    print(f"  {gap} ms em {at / 1000:.1f} s")
instants = sorted(at for _, at in late)
spacing = [b - a for a, b in zip(instants, instants[1:])]
if spacing:
    print(f"espaçamento mediano entre eles: {statistics.median(spacing) / 1000:.1f} s")
```

  Se os intervalos grandes se repetem a cada ~5 s, eles são o cálculo das janelas, e o valor é
  o tempo em que a tela e o toque ficam parados. Registre o máximo e a mediana. Eles também
  servem de medida do custo da janela dentro do app, para comparar com o
  `pipeline-window-benchmark` rodado na placa.
- [ ] **Resposta ao toque, por vídeo.** Filme a tela em câmera lenta (120 ou 240 quadros/s)
      durante 60 s com o dedo no sensor. Toque numa linha de sinal vital mais ou menos a cada
      segundo, para expandir e recolher. Conte os quadros entre o dedo encostar e a linha mudar,
      e separe os toques que coincidem com o salto da curva (a cada 5 s) dos demais. Registre os
      dois tempos.
- [ ] **Curva.** Observe se a varredura da pletismografia para ou salta a cada 5 s, e registre.
- [ ] Anote as três medidas. A decisão de tirar o cálculo da thread principal depende delas
      (pendência no README do app).

Se algum critério falhar, registre a medição e o critério que falhou antes de usar o equipamento.
