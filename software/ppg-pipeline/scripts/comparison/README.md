# Comparação na BIDMC: pipeline × PpgProcessor do app

O app `monitor-news2` pode calcular FC, SpO₂ e FR com dois métodos, escolhidos na compilação
(`MONITOR_NEWS2_PPG_ALGORITHM`):

- **pipeline**: o método deste diretório (MSPTD + Smart Fusion de Karlen; ver `../../README.md`);
- **processor**: o `PpgProcessor` do app (picos com limiar adaptativo; FR pela soma dos espectros
  das três modulações; ver `software/monitor-news2/src/core/PpgProcessor.h`).

Esta comparação usa o mesmo protocolo de validação do pipeline (`../bidmc/run_bidmc_validation.py`,
fases 2 e 3) para os dois métodos.

## Método

| Configuração | Entrada | Executável |
|---|---|---|
| `pipeline-125` | BIDMC na taxa nativa (125 Hz) | `tcc` |
| `pipeline-25` | BIDMC reduzida a 25 Hz | `tcc` |
| `processor-25` | BIDMC reduzida a 25 Hz | `ppg-processor-export` (monitor-news2) |

- **25 Hz** é a taxa que o MAX30102 entrega ao app (100 Hz com média de 4 no chip).
  `prepare_device_rate.py` reproduz isso com a média de cada bloco de 5 amostras. As colunas
  comparáveis entre si são as duas de 25 Hz. A de 125 Hz é a referência do método na taxa
  nativa.
- **Janelas e status.** O `ppg-processor-export` exporta na mesma grade de janelas do pipeline:
  primeira saída com 60 s de sinal, depois a cada 5 s; FC numa janela de 30 s e FR numa de 60 s.
  Ele usa os mesmos status (`aceita` / sem cálculo).
- **Nível DC artificial.** A BIDMC não traz intensidade bruta (`raw_intensity=false`). O
  exportador soma um nível DC fixo ao sinal para o detector de contato do app tratar o registro
  como dedo presente, sem alterar frequência nem forma do pulso.
- **Referências:** a FC contra a coluna HR do monitor, derivada do ECG. A FR contra o RESP do
  monitor (fase 2) e contra os dois anotadores manuais (fase 3), que são a referência principal
  de FR nessa base (ver `../../README.md`, seção 6).

## Resultados (53 registros, 2026-11-04)

MAE = erro absoluto médio. "Mediana por registro" é a forma usada na literatura com a BIDMC; o
IQR está entre parênteses.

| Métrica | pipeline-125 | pipeline-25 | processor-25 |
|---|---|---|---|
| FC: mediana do MAE por registro (bpm) | 0,57 (0,45–0,89) | **1,49** (1,19–2,00) | 3,21 (1,65–7,76) |
| FC: MAE agrupado / Pearson | 1,03 / 0,98 | **1,76 / 0,98** | 7,67 / 0,66 |
| FC: viés mediano (bpm) | −0,10 | −0,16 | −1,21 |
| FC: cobertura | 100% | 100% | 93% |
| FR × anotador 1: mediana do MAE (irpm) | 3,63 (1,55–5,78) | 3,61 (1,97–4,86) | **2,77** (1,49–4,56) |
| FR × anotador 2: mediana do MAE (irpm) | 3,73 | 3,72 | **2,85** |
| FR × anotador 1: viés agrupado | −2,93 | −2,78 | **−1,59** |
| FR × monitor: mediana do MAE (irpm) | 4,11 | 3,82 | **3,00** |
| FR: cobertura | 55% | 49% | **91%** |

- **FC**: o pipeline é claramente melhor. O PpgProcessor erra muito em parte dos registros
  (quartil superior acima de 7,7 bpm; correlação 0,66 contra 0,98). Reduzir de 125 para 25 Hz
  custa ~0,9 bpm ao pipeline.
- **FR**: o PpgProcessor é melhor. Tem menor erro, menos viés e cobertura de 91%, porque o Smart
  Fusion exige as três modulações concordando e descarta metade das janelas.
- **SpO₂**: não avaliada. A BIDMC só tem o canal infravermelho.
- O registro `bidmc07` não teve nenhuma janela comparável no PpgProcessor.

## Reproduzir

```sh
# no diretório software/ppg-pipeline, com as bases já preparadas (scripts/bidmc)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
cmake -S ../monitor-news2 -B ../monitor-news2/build-validation -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=<qt6>
cmake --build ../monitor-news2/build-validation --target ppg-processor-export
scripts/comparison/run_comparison.sh data/bidmc/prepared build/tcc \
    ../monitor-news2/build-validation/ppg-processor-export /tmp/comparacao-bidmc
```

Requer Python com numpy e pandas.
