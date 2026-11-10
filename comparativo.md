# Comparativo: PpgProcessor × ppg-pipeline

Os dois métodos de cálculo de FC, SpO₂ e FR a partir do PPG do MAX30102, como estão no app
`software/monitor-news2`. A escolha entre eles é feita na compilação
(`MONITOR_NEWS2_PPG_ALGORITHM`: `processor`, `pipeline` ou `hybrid`).

- **PpgProcessor**: `software/monitor-news2/src/core/PpgProcessor.{h,cpp}`
- **ppg-pipeline**: `software/ppg-pipeline` (no app, via `PipelineVitalsEstimator`)

## Visão geral

| | **PpgProcessor** (padrão da imagem) | **ppg-pipeline** |
| --- | --- | --- |
| Origem | Escrito para o app; ajustado com sinal sintético e gravações do MAX30102 | Desenvolvido sobre bases públicas; MSPTD traduzido do `ppg-beats` (MIT) |
| Modelo de cálculo | Contínuo: atualiza a cada batimento e publica a cada 1 s | Em janelas: buffer de 60 s, recalculado a cada 5 s |
| Taxa de amostragem | **25 Hz** fixos (o MAX30102 mede a 100 Hz e faz média de 4) | Taxa da fonte: validado a **86 Hz** (OpenOximetry) e **125 Hz** (BIDMC); no app roda a **25 Hz** |
| Detecção de dedo | Sim: IR > 50.000 entra, < 30.000 sai, confirmando 12 amostras para entrar e 8 para sair | Não tem; no app, quem detecta o dedo é o PpgProcessor |
| Primeira leitura | FC ~5 s, SpO₂ ~10 s, FR ~40–45 s | FC e SpO₂ ~35 s (60 s no `main.cpp` original); FR ≥ 60 s |

## Amostras analisadas por medida (a 25 Hz)

| Medida | PpgProcessor | ppg-pipeline |
| --- | --- | --- |
| FC | Últimos **8 batimentos** (~6–10 s, ~150–250 amostras) | Últimos **30 s = 750 amostras** |
| SpO₂ | Razão R dos últimos **8 batimentos regulares** (mínimo 4) | Últimos **30 s = 750 amostras** de cada canal |
| FR | Janela de **32 s**: 160 pontos a 5 Hz, ou 800 amostras originais | Janela de **60 s = 1.500 amostras**, reamostradas a 4 Hz (~240 pontos) |
| Frequência de cálculo | FC a cada batimento; FR a cada 5 s | FC, SpO₂ e FR a cada **5 s** (125 amostras novas) |

## Filtros e pré-processamento

| Etapa | PpgProcessor | ppg-pipeline |
| --- | --- | --- |
| Sinal cardíaco | Butterworth 2ª ordem, passa-alta **0,5 Hz** + passa-baixa **4 Hz**, no IR e no vermelho **invertidos** | Butterworth 2ª ordem, passa-alta **0,5 Hz** + passa-baixa **4 Hz**, no IR e no vermelho sem inverter |
| Partida do filtro | Começa já no nível do sinal (sem transitório inicial) e descarta 3 s | Começa do zero; o transitório faz a 1ª janela ser reprovada |
| Nível médio (DC) para a SpO₂ | Média móvel exponencial (α = 0,02, ~2 s) | Média do sinal bruto na janela de 30 s |
| Sinal respiratório | Passa-alta **0,13 Hz** + passa-baixa **0,6 Hz** em cada modulação, a 5 Hz | Sem filtro: subtração da média + janela de **Hamming** |
| Faixa de FR buscada | **0,13–0,6 Hz = 7,8–36 irpm** (fora das ondas de Mayer, ~6 irpm) | **0,1–0,5 Hz = 6–30 irpm** |

## Batimentos e frequência cardíaca

| | PpgProcessor | ppg-pipeline |
| --- | --- | --- |
| Detector | **Máximo local** em ±0,24 s, acima de 50% de uma amplitude que decai devagar (0,995 por amostra); no mínimo 0,3 s entre picos | **MSPTD**: busca picos em várias escalas de tempo ao mesmo tempo; melhor colocado no comparativo de Charlton (2022) |
| Precisão do instante do pico | Interpolação parabólica, abaixo dos 40 ms de uma amostra | Posição na amostra |
| Cálculo da FC | **Média** dos intervalos que estão a até ±25% da mediana | **Mediana** das FCs batimento a batimento na janela |
| Rejeição | Intervalos fora de 0,3–2 s; batimentos com intervalo ±30% fora da mediana ou amplitude fora de 0,5–2× ficam fora da SpO₂ e da FR | Janela inteira reprovada se a perfusão sair de **0,4–15%**; FC fora de 30–240 bpm; mínimo de 3 picos |

## SpO₂

| | PpgProcessor | ppg-pipeline |
| --- | --- | --- |
| Variação do pulso (AC) | Pico-a-pico do sinal filtrado **em cada batimento** | Pico-a-pico do sinal filtrado na **janela inteira de 30 s** |
| Razão R | (AC/DC do vermelho) ÷ (AC/DC do IR), **mediana de 8 batimentos** | Perfusão do vermelho ÷ perfusão do IR, na janela |
| Curva de conversão | **Maxim RD117**: −45,06·R² + 30,354·R + 94,845, limitada a 70–100% | **Linear**: 110 − 25·R (provisória), aceita 50–100% |
| Calibração | Nenhuma | Nenhuma |

## Frequência respiratória

| | PpgProcessor | ppg-pipeline |
| --- | --- | --- |
| Modulações usadas | Linha de base (média de cada 5 amostras), amplitude por batimento, intervalo por batimento | Amplitude (pico − pé), linha de base (IR bruto no pico), frequência (algoritmo de Berger) |
| Reamostragem | 5 Hz, interpolação linear entre batimentos regulares | 4 Hz, interpolação linear |
| Espectro | Remoção de tendência linear + **Hann**; DFT avaliada a cada 0,005 Hz, com interpolação parabólica do pico | Subtração da média + **Hamming**; DFT nos pontos naturais da janela (~1 irpm de passo em 60 s) |
| Fusão | **Soma dos espectros** normalizados das três modulações; o pico precisa ser máximo local fora da borda, com ≥ 25% da potência a ±1,5 irpm | **Smart Fusion** (Karlen 2013): exige **as três** estimativas e desvio-padrão ≤ 4 irpm, aí faz a média |
| Proteções | Abaixo de 4 batimentos por respiração, usa só a linha de base; mantém o último valor por até 20 s | Mínimo de 20 picos; janela descartada se algum intervalo sair de 230–2400 ms |

## Resultados

BIDMC: 53 registros reduzidos a 25 Hz (`software/ppg-pipeline/scripts/comparison`). "Seu dedo":
gravação de 120 s do MAX30102 pelo acesso IIO, com referência calculada no PC (FC espectral
~64 bpm, FR espectral ~12,6 irpm).

| Teste | PpgProcessor | ppg-pipeline |
| --- | --- | --- |
| **BIDMC, FC**: mediana do erro (25 Hz) | 3,21 bpm | **1,49 bpm** (0,57 a 125 Hz) |
| **BIDMC, FR** contra anotador | **2,77 irpm** | 3,61 irpm |
| BIDMC: cobertura da FR | **91%** | 49% |
| Dedo: FC (referência ~64) | 61–72, mediana 64; **errou 38 e 55** logo após encostar o dedo | **60–68, mediana 65**, sem erros |
| Dedo: SpO₂ | 97–100% (mediana 99) | 96–98% (mediana 96) |
| Dedo: FR (referência ~12,6) | **14 irpm** | **6–7,5 irpm** (onda de Mayer) ou sem leitura |
| Custo computacional | Baixo: picos por janela de 13 amostras e uma DFT pequena a cada 5 s | Maior: o MSPTD monta matrizes de ~(N/2 × N) a cada 5 s, mas roda tranquilo a 25 Hz |

## Resumo

- **FC**: o **ppg-pipeline** é melhor: erra menos e é estável. O PpgProcessor aparece mais
  rápido, mas erra logo depois que o dedo encosta.
- **FR**: o **PpgProcessor** é melhor: menos erro, mais cobertura e não confunde a onda de Mayer
  com respiração.
- **SpO₂**: sem vencedor. Os dois usam a mesma razão R e diferem só na curva de conversão (~3
  pontos de diferença); falta comparar com um oxímetro de referência.
- **Tempo de resposta**: o PpgProcessor mostra a primeira FC em ~5 s; o pipeline, em ~35 s.
