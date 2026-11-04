# Cálculo de parâmetros fisiológicos - Sistema Embarcado para Monitoramento e Predição de Deterioração Clínica Intra-hospitalar

## 1. Visão geral

Este projeto é o pipeline C++17 de um TCC que processa fotopletismografia, chamada de PPG, para calcular frequência cardíaca, SpO2 e frequência respiratória, e roda numa Raspberry Pi. O executável `tcc` deste diretório lê os dois canais a partir de um CSV preparado das bases públicas, para desenvolvimento e validação. No equipamento, o app `software/monitor-news2` lê o MAX30102, usa este método como biblioteca e calcula o NEWS2 com os parâmetros resultantes (ver seção 7).

## 2. Como usar

As dependências são CMake 3.16 ou mais novo, um compilador C++17, e um ambiente Python com pandas, numpy, scipy e wfdb para os scripts de preparação e validação.

```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
ctest
```

O build em Release é o usado na Raspberry, porque a DFT direta da frequência respiratória e o MSPTD ficam bem mais lentos sem otimização. Nenhum teste usa `assert`, que seria desativado por `NDEBUG` nesse modo, então o `ctest` continua validando normalmente em Release.

Pra rodar o pipeline, aponte `PPG_MOCK_FILE` pro CSV preparado e, se quiser exportar os parâmetros por janela, defina `PPG_PARAM_EXPORT=1` e `PPG_PARAM_EXPORT_PATH`:

```
PPG_MOCK_FILE=caminho.csv PPG_PARAM_EXPORT=1 PPG_PARAM_EXPORT_PATH=saida.csv ./build/tcc
```

As validações ficam em `scripts/openoximetry` e `scripts/bidmc`, cada uma com um script de preparação e um de execução da validação contra a referência da base.

## 3. Arquitetura

```
CSV de entrada + .meta
        |
read_ppg_sample (canal vermelho, canal infravermelho)
        |
filtro passa-banda (um estado por canal)
        |
PPGBuffer: janela de 60 s, atualizada a cada 5 s
        |
        +-- últimos 30 s --> controle de qualidade --> FC (IR) e SpO2 (os dois canais)
        |
        +-- 60 s inteiros --> controle de qualidade --> três modulações + Smart Fusion --> FR
```

```
include/, src/        módulos do pipeline C++ e main.cpp
tests/                 um executável de teste por módulo
scripts/common/        núcleo de validação compartilhado
scripts/openoximetry/  preparação e validação do OpenOximetry
scripts/bidmc/         preparação e validação da BIDMC
data/, debug_output/   dados e resultados, organizados por base
```

| Módulo | Papel |
|---|---|
| `PPG` | Constantes do método em segundos |
| `PPGBuffer` | Buffer deslizante e conversão de segundos em amostras |
| `PPGFilter` | Filtro passa-banda |
| `PPGQuality` | Índice de perfusão e regra de artefato por período de pulso |
| `PPGPeaks` / `PPGMsptdBeatDetector` | Detecção de batimentos |
| `PPGHeartRate` | Frequência cardíaca |
| `PPGSpO2` | SpO2 |
| `PPGRespiratorySignal` / `PPGRespiratoryRate` | Modulações respiratórias e Smart Fusion |
| `PPGSource` / `PPGSourceMock` | Leitura da fonte de dados genérica |
| `PPGDebug` | Exportação dos parâmetros em CSV |
| `DS18B20` | Leitura de temperatura |
| `main` | Orquestração do laço principal |

## 4. Formato de entrada

O CSV de entrada tem a coluna `ir_signal` obrigatória e `red_signal` opcional, e sem o canal vermelho o SpO2 fica desativado. Ao lado do CSV fica um arquivo `.meta`, com a chave `sample_rate_hz` obrigatória trazendo a taxa real da fonte, e a chave `raw_intensity`, opcional e assumida verdadeira, que declara se o canal infravermelho é a intensidade bruta do fotodetector. Essa declaração vem da documentação da base ou do sensor, e quando é falsa o critério de PI do controle de qualidade deixa de ser aplicado para FC e FR, porque o índice de perfusão só tem significado físico de perfusão sobre um sinal bruto.

## 5. Processamento

**Janelas e buffer.** As durações de 30 s, 60 s e do hop de 5 s são definidas em segundos e convertidas em amostras pela taxa real da fonte, com arredondamento pro inteiro mais próximo. Existe um único buffer de 60 s, lido nos últimos 30 s para FC e SpO2 e por inteiro para FR, e esse tamanho de janela de 30 s também é empregado por Feli et al. (2023).

**Filtro.** O sinal passa por um Butterworth passa-banda de 0,5 a 4 Hz, em dois biquads de segunda ordem, com estado mantido entre amostras. Essa faixa corresponde a 30 e 240 bpm, e cobre a componente pulsátil do PPG, que fica em torno de 1 Hz segundo Allen (2007).

**Qualidade.** O PI de cada canal é a amplitude pico a pico do sinal filtrado dividida pela média do sinal bruto, aceito entre 0,4% e 15%. O piso fica abaixo da faixa típica de pacientes críticos segundo Sun et al. (2024), e o teto acima da faixa normal relatada por Coutrot et al. (2021). A decisão é por parâmetro, com FC e FR usando só o IR, em 30 s e 60 s respectivamente, e SpO2 exigindo os dois canais, e `raw_intensity` desativa esse critério para FC e FR quando o sinal não é intensidade bruta.

**Batimentos.** Os picos são detectados pelo MSPTD de Bishop e Ercole (2018), selecionado a partir do comparativo de Charlton et al. (2022). A tradução para C++17 veio do ppg-beats, sob licença MIT, e foi verificada idêntica ao original em GNU Octave sobre oito trechos de sinal. Limitar o refinamento de posição às bordas da janela é uma adaptação própria, já que o original não verifica esse limite.

**FC.** A frequência instantânea entre dois picos é 60 dividido pelo intervalo em segundos, e o valor da janela é a mediana dessas frequências. A janela é rejeitada com menos de 3 picos ou fora de 30 a 240 bpm.

**SpO2.** A razão R é o PI do vermelho dividido pelo PI do infravermelho, e o SpO2 é 110 menos 25 vezes R, com esses dois coeficientes provisórios. Um resultado fora de 50% a 100% é descartado.

**FR.** O método segue as três modulações de Karlen et al. (2013): amplitude como pico menos pé, linha de base como o valor bruto no pico, e frequência pelos intervalos entre picos, processados pelo algoritmo de Berger et al. (1986). As três são reamostradas a 4 Hz, seguidas de subtração da média, janela de Hamming, DFT direta e busca do máximo entre 0,1 e 0,5 Hz. Essa faixa contém os limiares de pontuação do NEWS2 e o limite de 0,5 Hz fica abaixo das frequências cardíacas típicas, em torno de 1 Hz. A regra de artefato de 230 a 2400 ms entre picos consecutivos vem de Karlen et al. (2013), e o Smart Fusion aceita a média das três estimativas quando o desvio padrão entre elas fica até 4 rpm. São adaptações próprias aplicar a regra de artefato à janela inteira de 60 s, subtrair a média antes da DFT, exigir um mínimo de 20 picos, exigir as três modulações presentes antes do Smart Fusion, e usar desvio padrão populacional nesse limite de 4 rpm.

| Parâmetro | Valor | Origem | Arquivo |
|---|---|---|---|
| Janela de FC e SpO2 | 30 s | Feli et al. (2023) | `PPG.hpp` |
| Janela de FR | 60 s | decisão própria | `PPG.hpp` |
| Hop | 5 s | decisão própria | `PPG.hpp` |
| Filtro passa-banda | 0,5 a 4 Hz | faixa fisiológica de 30 a 240 bpm, com centro em torno de 1 Hz segundo Allen (2007) | `PPGFilter.hpp` |
| PI minimo / máximo | 0,4% / 15% | Sun et al. (2024) / Coutrot et al. (2021) | `PPGQuality.hpp` |
| Período de pulso válido | 230 a 2400 ms | Karlen et al. (2013) | `PPGQuality.hpp` |
| FC aceita | 30 a 240 bpm | decisão própria | `PPGHeartRate.hpp` |
| FC, mínimo de picos | 3 | decisão própria | `PPGHeartRate.hpp` |
| SpO2, coeficientes | 110 e 25 | literatura geral, provisório | `PPGSpO2.hpp` |
| SpO2 aceito | 50% a 100% | decisão própria | `PPGSpO2.hpp` |
| FR, faixa de busca | 0,1 a 0,5 Hz (6 a 30 rpm) | limiares do NEWS2, decisão própria, limite superior também usado por Argüello-Prada et al. (2024) | `PPGRespiratorySignal.hpp` |
| FR, reamostragem | 4 Hz | Karlen et al. (2013) | `PPGRespiratoryRate.hpp` |
| FR, mínimo de picos | 20 | decisão própria | `PPGRespiratoryRate.hpp` |
| Smart Fusion, limite | 4 rpm, desvio populacional | Karlen et al. (2013), interpretação própria | `PPGRespiratoryRate.hpp` |

## 6. Validação

O OpenOximetry foi a base de desenvolvimento e a BIDMC a base de teste, usada só depois do método congelado. O núcleo de validação, em `scripts/common/ppg_validation.py`, alinha cada janela com a referência por intervalo de tempo, agrega mais de uma amostra de referência pela mediana, e classifica cada janela como comparável, descartada, sem cálculo ou sem referência.

| Base | Parâmetro | MAE | Viés | Pearson | Cobertura |
|---|---|---|---|---|---|
| OpenOximetry | FC | 5,46 bpm | -4,66 | 0,55 | 88% |
| OpenOximetry | SpO2 | 3,19 pp | -1,15 | 0,92 | 84% |
| OpenOximetry | FR fundida | 11,12 rpm | -11,10 | -0,11 | 16% |
| BIDMC | FC | 0,57 bpm (mediana) | -0,10 | 0,98 | 100% |
| BIDMC | FR fundida, vs. monitor | 4,11 rpm (mediana) | -3,03 | 0,11 | 55% |
| BIDMC | FR fundida, vs. anotador 1 | 3,63 rpm (mediana) | -3,15 | 0,38 | 55% |
| BIDMC | FR fundida, vs. anotador 2 | 3,73 rpm (mediana) | -3,18 | 0,37 | 55% |

Os valores do OpenOximetry são calculados sobre todas as janelas de um único encounter, enquanto os da BIDMC são a mediana por registro dos 53 registros da base. Na BIDMC a referência de FC é a coluna HR, derivada do ECG pelo próprio monitor.

Na BIDMC, os dois anotadores manuais de respiração concordam entre si em 98,4% das janelas dentro de 2 rpm de diferença, enquanto o monitor diverge de cada um deles nessa margem em cerca de 11,5% das janelas, por isso tratamos as anotações manuais como referência principal de FR nessa base.

A tabela 4 de Argüello-Prada et al. (2024) relata, na BIDMC, mediana de MAE de 2,3 rpm para Shelley et al., 2,7 para Pimentel et al. (2017), 3,2 para o próprio método de Argüello-Prada et al., 4,6 para Nilsson et al., 5,5 para Fleming e Tarassenko e 5,7 para o Smart Fusion de Karlen et al. (2013). Essa comparação não é direta com os números acima, porque vem de outra implementação e outro alinhamento de janela.

## 7. Uso no app

O app `software/monitor-news2` usa este projeto como biblioteca (`ppg_pipeline`, via
`add_subdirectory`): o executável `tcc` e os testes só são montados quando este diretório é
compilado sozinho. O app lê o MAX30102 (i2c-dev ou driver IIO) e entrega as amostras a 25 Hz ao
`PipelineVitalsEstimator`, que reproduz o laço do `main.cpp` (filtro, buffer de 60 s, qualidade,
FC, SpO₂ e FR a cada 5 s). Por padrão o app usa FC e SpO₂ deste método e a FR do seu próprio
`PpgProcessor`; a comparação que embasa essa escolha está em `scripts/comparison`.

O módulo `DS18B20` lê o sensor real em `/sys/bus/w1/devices/28-*/w1_slave` quando ele existe
(`find_ds18b20_w1_slave`, `read_ds18b20_w1_slave`), e o arquivo de exemplo quando não.

## 8. Limitações e pendências

- Coeficientes de SpO2 provisórios, sem calibração própria.
- Viés negativo da FR no OpenOximetry e na BIDMC.
- Ausência de estimativa de FR acima de 30 rpm, sendo que a pontuação máxima do NEWS2 já vale a partir de 25 rpm.
- Critério de PI não avaliado na BIDMC, porque `raw_intensity` fica sempre falso nessa base.
- Desempenho fora de repouso não avaliado em nenhuma das duas bases.
- No `tcc`, temperatura lida só no início da execução; o app lê periodicamente.
- No sinal real do MAX30102 a 25 Hz, a FR fundida caiu em 6–7,5 irpm (ondas de Mayer), por isso
  o app usa a FR do `PpgProcessor` por padrão (ver `scripts/comparison`).
- Falta confirmar se o simulador Waller simula SpO2 de forma óptica.
- SpO2 avaliado num único encounter do OpenOximetry.
- Todo o desenvolvimento do método foi feito sobre esse mesmo encounter.

## 9. Referências

ALLEN, J. Photoplethysmography and its application in clinical physiological measurement. Physiological Measurement, v. 28, n. 3, p. R1-R39, 2007.

ARGÜELLO-PRADA, E. J.; MARCILLO IBARRA, K. D.; DÍAZ JIMÉNEZ, K. L. The use of successive systolic differences in photoplethysmographic (PPG) signals for respiratory rate estimation. Heliyon, v. 10, n. 4, e26036, 2024. DOI: 10.1016/j.heliyon.2024.e26036.

BERGER, R. D.; AKSELROD, S.; GORDON, D.; COHEN, R. J. An efficient algorithm for spectral analysis of heart rate variability. IEEE Transactions on Biomedical Engineering, v. 33, n. 9, p. 900-904, 1986.

BISHOP, S. M.; ERCOLE, A. Multi-scale peak and trough detection optimised for periodic and quasi-periodic neuroscience data. In: HELDT, T. (Ed.). Intracranial Pressure and Neuromonitoring XVI. Acta Neurochirurgica Supplement, v. 126. Springer, 2018. p. 189-195. DOI: 10.1007/978-3-319-65798-1_39.

CHARLTON, P. H. et al. Detecting beats in the photoplethysmogram: benchmarking open-source algorithms. Physiological Measurement, v. 43, n. 8, 085007, 2022. DOI: 10.1088/1361-6579/ac826d.

CHARLTON, P. H. ppg-beats: algorithms to detect heartbeats in photoplethysmogram (PPG) signals. Zenodo, 2022. DOI: 10.5281/zenodo.6037646.

COUTROT, M.; DUDOIGNON, E.; JOACHIM, J.; GAYAT, E.; VALLÉE, F.; DÉPRET, F. Perfusion index: Physical principles, physiological meanings and clinical implications in anaesthesia and critical care. Anaesthesia Critical Care & Pain Medicine, v. 40, n. 6, 100964, 2021. DOI: 10.1016/j.accpm.2021.100964.

FELI, M. et al. 2023. Referência completa a ser preenchida pelo autor.

KARLEN, W.; RAMAN, S.; ANSERMINO, J. M.; DUMONT, G. A. Multiparameter respiratory rate estimation from the photoplethysmogram. IEEE Transactions on Biomedical Engineering, v. 60, n. 7, p. 1946-1953, 2013. DOI: 10.1109/TBME.2013.2246160.

PIMENTEL, M. A. F. et al. Toward a robust estimation of respiratory rate from pulse oximeters. IEEE Transactions on Biomedical Engineering, v. 64, n. 8, p. 1914-1923, 2017. DOI: 10.1109/TBME.2016.2613124.

ROYAL COLLEGE OF PHYSICIANS. National Early Warning Score (NEWS) 2: standardising the assessment of acute-illness severity in the NHS. London: RCP, 2017.

SUN, X. et al. Peripheral perfusion index of pulse oximetry in adult patients: a narrative review. European Journal of Medical Research, v. 29, 457, 2024. DOI: 10.1186/s40001-024-02048-3.

BIDMC PPG and Respiration Dataset, versão 1.0.0. PhysioNet. Disponível em: https://physionet.org/content/bidmc/1.0.0/.

OpenOximetry Repository, versão 1.1.1. PhysioNet. Disponível em: https://physionet.org/content/openox-repo/1.1.1/.
