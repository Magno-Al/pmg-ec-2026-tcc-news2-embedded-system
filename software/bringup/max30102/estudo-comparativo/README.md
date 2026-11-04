# Estudo comparativo: MAX30102 por i2c-dev × driver IIO do kernel

Data: 2026-11-04. Raspberry Pi 3B+, kernel 6.6.28, imagem Buildroot do projeto. Um voluntário
em repouso com o dedo no sensor durante todas as medições.

## Pergunta

O monitor pode falar com o MAX30102 de duas formas, escolhidas na compilação do app
(`MONITOR_NEWS2_SENSOR_BACKEND`):

| | **i2c-dev** (espaço de usuário) | **IIO** (driver do kernel) |
|---|---|---|
| Quem configura o chip | o app (`src/sensors/Max30102.cpp`) | o driver `drivers/iio/health/max30102.c` |
| Como chegam as amostras | o app lê a FIFO do chip a cada 40 ms | interrupção no GPIO 17 → driver → `/dev/iio:device0` |
| Configuração do chip | 100 Hz, média de 4 → 25 Hz | 400 Hz, média de 4 → 100 Hz; o app faz média de 4 → 25 Hz |
| ADC / pulso / LEDs | 4096 nA / 411 µs / ~7,2 mA | 4096 nA / 411 µs / 7,0 mA (fixos no driver) |

Ambas entregam 25 amostras/s ao mesmo `PpgProcessor`, que calcula FC, SpO₂ e FR. A pergunta é
se a forma de acesso muda a qualidade do sinal, as medidas, o custo ou a robustez.

## Método

- **Gravador** (`../PpgBackendRecorder.cpp`): usa as mesmas classes de acesso do app e grava cada
  amostra com o instante da leitura (`CLOCK_MONOTONIC`), além do tempo de CPU e das trocas de
  contexto (`getrusage`). Reproduz o ritmo do app: sono de 40 ms no i2c-dev, `poll()` no IIO.
- **Protocolo**: segmentos de 75 s alternando os acessos, porque os dois não podem ler o chip ao
  mesmo tempo. Rodada 1: i2c-dev, IIO, i2c-dev, IIO. Rodada 2: ordem invertida. Alternar evita
  que a variação natural da FC ao longo do tempo pareça efeito do acesso.
- **Análise** (`scripts/analisar.py`, ignora os 8 s iniciais de cada segmento):
  - entrega: taxa efetiva, despertares/s, variação do atraso de entrega;
  - custo: CPU e trocas de contexto;
  - sinal: DC, amplitude cardíaca (passa-banda 0,5–4 Hz), índice de perfusão e SNR (banda
    cardíaca × ruído acima de 5 Hz);
  - medidas: FC, SpO₂ e FR do próprio `PpgProcessor` do app (`scripts/replay_ppg.cpp`), uma
    por segundo.
- **Atraso absoluto**: ponteiros da FIFO do chip lidos com `i2cget -f` durante cada acesso. O
  número de amostras paradas no chip é o atraso que o timestamp de leitura não enxerga.

## Resultados

Médias dos segmentos válidos (i2c-dev: 4; IIO: 3).

| Métrica | i2c-dev | IIO | Leitura |
|---|---|---|---|
| Taxa efetiva | 24,88 Hz | 24,70 Hz | oscilador do chip; cada modo erra diferente |
| Despertares do processo | 24,6/s | 98,9/s (75% sem amostra nova) | IIO acorda a cada amostra de 100 Hz |
| CPU do leitor | 0,22% | 0,25% | desprezível nos dois (o app inteiro usa ~9%) |
| Amostras paradas na FIFO do chip | 0–1 (a 25 Hz) | 29–30 (a 100 Hz) | ver "atraso" |
| **Atraso sensor → app** | **~0–80 ms** | **~300 ms fixos** | driver só interrompe com a FIFO quase cheia |
| SNR cardíaco | 16,1 dB | 16,3 dB | equivalente |
| Índice de perfusão | 2,4–3,6% | 2,8–3,7% | acompanha o dedo, não o acesso |
| FC | 70,1 bpm (68,0–74,4) | 70,1 bpm (68,9–71,2) | idêntica |
| SpO₂ | 99–100% | 99,8–100% | idêntica |
| FR | 18,0 irpm (17,2–19,2) | 17,9 irpm (17,3–18,4) | idêntica |
| Falhas | 0 em 4 segmentos | **1 em 5 segmentos: 0 amostras em 75 s** | ver "robustez" |

Tabela completa por segmento: `python3 scripts/analisar.py dados/rodada1,dados/rodada2 <replay>`
(as gravações não estão no repositório; ver "Arquivos").

## Discussão

**Sinal e medidas: sem diferença.** SNR, perfusão, FC, SpO₂ e FR coincidem dentro da variação
entre segmentos do mesmo acesso. A média extra do IIO (16 leituras a 400 Hz contra 4 a 100 Hz)
não melhorou o SNR de forma mensurável: o ruído dominante está no próprio dedo, não no ADC.

**Atraso: o i2c-dev é melhor.** O driver configura a interrupção para "FIFO quase cheia" e
retira uma amostra por vez, então a FIFO do chip fica com ~30 amostras (300 ms) paradas. Para o
monitor isso significa curva e números ~0,3 s atrasados. Não é crítico para NEWS2, que trabalha
em minutos, mas é uma diferença objetiva, e só muda alterando o driver do kernel.

**Margem contra transbordo: o i2c-dev é melhor.** Com a FIFO em 30 de 32, o IIO tem ~20 ms de
folga: se a thread de interrupção atrasar mais que isso, por exemplo com a CPU ocupada pela
inferência do modelo, o chip descarta amostras. No i2c-dev a FIFO fica quase vazia, com folga
de ~1,2 s.

**Robustez: falha intermitente no IIO.** Em 1 de 5 segmentos longos o IIO não entregou nenhuma
amostra; em 11 testes curtos depois disso, nenhuma falha. O driver usa interrupção por borda de
descida e não lê o status do chip antes de registrá-la. A hipótese é que, se o pino INT já
estiver baixo nesse instante, a borda se perde e o driver fica mudo. A causa não foi
confirmada: a falha não se repetiu com a instrumentação ligada. O app tem um vigia que reconecta
após 3 s sem amostras, mas a recuperação desse caso específico não foi testada.

**Custo: empate na prática.** O IIO acorda o processo 4× mais (cada amostra de 100 Hz), e o CPU
fica em 0,25% contra 0,22%. Ajustar `buffer/watermark` reduziria os despertares, mas não o
atraso.

**Taxa efetiva.** O `PpgProcessor` assume 25,00 Hz. O erro de base de tempo é de 0,5%
(i2c-dev) e 1,2% (IIO), o que dá até +0,8 bpm a 70 bpm. Pode ser compensado medindo a taxa real.

## Conclusão

Para este projeto, o **i2c-dev é a melhor escolha**: mesma qualidade de medida, ~4× menos
atraso, mais folga contra perda de amostras, nenhuma falha observada e configuração do chip sob
controle do app. O IIO funciona e tem a vantagem de usar a interface padrão do Linux, mas
depende do pino INT, fixa a configuração no kernel e mostrou uma falha intermitente. Fica como
alternativa documentada.

## Limitações

- Um voluntário, em repouso, ~10 min de registro no total, sem referência clínica (oxímetro ou
  contagem respiratória) para a exatidão absoluta: o estudo compara os acessos entre si.
- A falha do IIO aconteceu uma vez, e a causa é hipótese.
- O CPU foi medido no gravador, não no app completo com interface.

## Arquivos

As gravações (`dados/`) são sinal fisiológico do voluntário e ficam fora do repositório
(`.gitignore`). Para refazer a análise, rode os protocolos no equipamento e copie os arquivos
gerados em `/root/comparativo*` para `dados/rodada*`.

| Caminho | Conteúdo |
|---|---|
| `dados/rodada*/seg*-<acesso>.csv` | amostras: instante da leitura, tamanho da rajada, vermelho, IR |
| `dados/rodada*/seg*-resumo.txt` | duração, taxa, leituras, CPU, trocas de contexto |
| `dados/rodada*/log.txt` | horários do protocolo |
| `scripts/protocolo-rodada*.sh` | scripts executados no Pi |
| `scripts/analisar.py` | análise (numpy/scipy) |
| `scripts/replay_ppg.cpp` | passa uma gravação pelo `PpgProcessor` do app |
| `../PpgBackendRecorder.cpp` | gravador |
