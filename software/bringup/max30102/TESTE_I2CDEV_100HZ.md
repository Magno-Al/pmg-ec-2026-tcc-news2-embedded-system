# Teste do i2c-dev a 100 Hz e troca do acesso padrão

O acesso por i2c-dev (`software/monitor-news2/src/sensors/Max30102.cpp`) passou a configurar o
MAX30102 como o driver IIO do kernel: 400 Hz internos, média de 4 na FIFO, ou seja, 100
amostras/s. Ele entrega as amostras de 100 Hz ao ppg-pipeline (FC do modo `refined`) e a média
de cada 4, a 25 Hz, ao `PpgProcessor`. Também conta as amostras que o chip descarta quando a FIFO
enche (contador de overflow).

Isso foi verificado só com testes automatizados, sem o sensor. Este roteiro valida o i2c-dev no
equipamento e, se ele for aprovado, faz dele o acesso padrão da imagem no lugar do IIO.

Motivo da troca: no estudo comparativo (`estudo-comparativo/`), o i2c-dev teve a mesma
qualidade de sinal que o IIO, menos atraso, mais folga contra transbordo da FIFO e nenhuma
falha. O IIO teve uma falha de aquisição e mantém a FIFO do chip quase cheia.

**Muda com o i2c-dev a 100 Hz.** A FIFO do chip guarda 32 amostras, ou seja, 320 ms a 100 Hz,
contra 1,28 s a 25 Hz. É o único buffer entre o chip e o app, que a lê a cada 40 ms na thread
principal. Se a thread principal ficar travada por mais de cerca de 280 ms, amostras se perdem.
Esse é o principal ponto a observar.

## 0. Antes de ir para a bancada

- [ ] Confirmar no datasheet do MAX30102 (tabela de configurações permitidas no modo SpO₂, taxa
      × largura de pulso) que **400 amostras/s com pulso de 411 µs** é uma combinação aceita.
      O driver IIO do kernel usa a mesma combinação, mas ela não foi conferida na tabela.
- [ ] Confirmar no datasheet o comportamento da FIFO cheia com `FIFO_ROLLOVER_EN = 1`. O código
      assume que, com overflow, os ponteiros de escrita e leitura ficam iguais e
      `OVF_COUNTER > 0`, e aí lê as 32 amostras. Ver `Max30102Fifo::availableSampleCount`.
- [ ] No PC, com o Qt 6.5 ou mais novo: `ctest --test-dir build` passa, incluindo
      `tst_max30102fifo` e `tst_ppgdecimator`.

## 1. Compilar o app com i2c-dev e o modo refined

Não altere o `rpi3_news2_defconfig` nesta etapa: a troca é só local, no `.config` do build.

```sh
cd buildroot
make menuconfig
#   External options → monitor-news2
#     acesso ao MAX30102  → i2c-dev (espaco de usuario, polling)
#     calculo de FC/SpO2/FR → PpgProcessor + FC refinada a 100 Hz pelo ppg-pipeline
make monitor-news2-dirclean monitor-news2
```

**Não rode `make save`**: ele gravaria essa escolha no defconfig.

O binário fica em `buildroot/build/target/root/monitor-news2`. Copie-o com outro nome, para não
substituir o da imagem:

```sh
scp build/target/root/monitor-news2 root@<ip>:/root/monitor-news2-i2cdev
```

Compile também a ferramenta de medição de tempo e copie-a para o equipamento. O comando está no
`README.md` desta pasta (`pipeline-window-benchmark`).

```sh
scp software/bringup/max30102/pipeline-window-benchmark root@<ip>:/root/
```

Para voltar ao estado anterior do build, escolha de novo "driver IIO" no `menuconfig` e recompile
o pacote.

## 2. Parar o driver IIO

O driver IIO e o i2c-dev não podem usar o chip ao mesmo tempo: com o módulo `max30102`
carregado, o endereço 0x57 fica ocupado. No equipamento:

```sh
killall monitor-news2 2>/dev/null        # se o app da imagem estiver aberto
/etc/init.d/S30max30102 stop             # descarrega o módulo; o buffer IIO apaga os LEDs
ls /sys/bus/iio/devices/                 # não deve listar nenhum iio:device do max30102
i2cdetect -y 1                           # 0x57 deve aparecer como "57", não como "UU"
```

O `S30max30102` carrega o módulo de novo a cada boot. Depois de reiniciar, repita este passo.

## 3. Medir durante pelo menos 5 minutos

Com o voluntário em repouso, o dedo parado no sensor desde o início e um oxímetro comercial no
outro dedo como referência de FC:

```sh
MONITOR_NEWS2_TRACE=/tmp/i2cdev.csv /root/monitor-news2-i2cdev --fullscreen 2>&1 | tee /tmp/i2cdev.log
```

Enquanto o app roda, em outro terminal, confirme a configuração do chip. O registrador 0x0A deve
valer `0x2f`; antes da mudança valia `0x27`.

```sh
i2cget -f -y 1 0x57 0x0a
```

Encerre o app depois de pelo menos 5 minutos de contato. Depois, com o app fechado, meça o tempo
de cálculo das janelas:

```sh
/root/pipeline-window-benchmark 10
```

A linha `refined` dá a média e o máximo por janela da FC a 100 Hz. Repita a medição com o app
aberto, para ver o efeito da concorrência com a interface.

Para comparar com o IIO, repita a medição com o app da imagem
(`/etc/init.d/S30max30102 start`, depois `/root/monitor-news2` com outro arquivo de traço).
Alterne os dois acessos, como no estudo comparativo.

### O que tirar do traço

Copie `/tmp/i2cdev.csv` e `/tmp/i2cdev.log` para o PC. As linhas do traço são:

- `s,ms,red,ir,contato,...`: cada amostra de 25 Hz entregue ao `PpgProcessor`;
- `p,ms,fc,spo2,fr,fc_processor,fc_refinada,fr_bruta,correcao,amostras_perdidas`: cada
  publicação, uma vez por segundo.

| Medida | Como obter |
|---|---|
| Taxa efetiva (100 Hz) | `100 × correcao` na última linha `p`. A correção é a taxa medida a 25 Hz dividida por 25, e vale a partir de 20 s; o fator é limitado a ±5%. Confira também pela contagem de linhas `s` × 4 dividida pela duração. |
| Amostras perdidas | Última coluna das linhas `p`. Avisos `FIFO do chip transbordou` no log. |
| FC exibida | Coluna `fc`. `fc_refinada` é a do ppg-pipeline a 100 Hz, `fc_processor` é a do `PpgProcessor`. |
| Tempo até a 1ª FC refinada | `ms` da primeira linha `p` com `fc_refinada > 0`, menos o `ms` da primeira linha `s` com `contato = 1`. |
| Tempo de cálculo por janela | Saída do `pipeline-window-benchmark` (média e máximo da linha `refined`). |
| Falhas de leitura | Linhas `MAX30102: ... reconectando` no log. |

Resumo automático, no PC:

```python
import csv, statistics, sys

samples, publications = [], []
for row in csv.reader(open(sys.argv[1])):
    if row and row[0] == "s":
        samples.append(row)
    elif row and row[0] == "p":
        publications.append(row)

contact_ms = next(int(r[1]) for r in samples if r[4] == "1")
duration_s = (int(samples[-1][1]) - int(samples[0][1])) / 1000
refined = [r for r in publications if float(r[6]) > 0]
heart_rates = [int(r[2]) for r in refined]

print(f"duração: {duration_s:.0f} s, amostras de 25 Hz: {len(samples)}")
print(f"taxa pela contagem: {4 * len(samples) / duration_s:.2f} Hz")
print(f"taxa pela correção: {100 * float(publications[-1][8]):.2f} Hz")
print(f"amostras perdidas: {publications[-1][9]}")
print(f"1ª FC refinada: {(int(refined[0][1]) - contact_ms) / 1000:.1f} s após o contato")
print(f"FC refinada: média {statistics.mean(heart_rates):.1f}, "
      f"desvio {statistics.pstdev(heart_rates):.1f}, "
      f"mín {min(heart_rates)}, máx {max(heart_rates)} bpm")
jumps = [abs(a - b) for a, b in zip(heart_rates, heart_rates[1:])]
print(f"maior salto entre segundos: {max(jumps)} bpm")
print(f"segundos com contato e sem FC refinada após a 1ª: "
      f"{sum(1 for r in publications if int(r[1]) > int(refined[0][1]) and float(r[6]) == 0)}")
```

Salve como `resumo_traco.py` e rode `python3 resumo_traco.py i2cdev.csv`.

## 4. Critérios para aprovar a troca

Todos devem valer numa medição de pelo menos 5 minutos com o dedo parado. De preferência, em
duas rodadas.

- [ ] Registrador 0x0A = `0x2f` durante a medição.
- [ ] Taxa efetiva entre **98 e 101 Hz**, pela correção e pela contagem.
- [ ] **Nenhuma amostra perdida**: `amostras_perdidas = 0` e nenhum aviso de transbordo no log.
- [ ] Nenhuma reconexão (`reconectando`) no log.
- [ ] Primeira FC refinada em até **40 s** após o contato. O esperado é de 30 a 35 s: a janela de
      30 s costuma ser reprovada pelo transitório do filtro, e a seguinte sai em 35 s.
- [ ] FC estável depois da primeira FC refinada:
  - desvio padrão de até 3 bpm em repouso;
  - nenhum salto maior que 10 bpm entre segundos consecutivos;
  - nenhum segundo sem FC refinada com o dedo parado, a não ser o fallback ocasional para o
    `PpgProcessor` quando uma janela é reprovada.
- [ ] FC média a até 2 bpm da do oxímetro de referência e da medição pelo IIO na mesma sessão.
- [ ] Tempo de cálculo por janela na Raspberry com máximo bem abaixo da folga da FIFO, por
      exemplo **< 50 ms**. A folga é de cerca de 280 ms: 320 ms da FIFO menos os 40 ms entre
      leituras.

Se algum critério falhar, mantenha o IIO como padrão e registre o resultado em
`estudo-comparativo/`.

## 5. Tornar o i2c-dev o padrão (depois de aprovado)

1. **Defconfig** (`buildroot/external/configs/rpi3_news2_defconfig`): troque
   `BR2_PACKAGE_MONITOR_NEWS2_SENSOR_IIO=y` por `BR2_PACKAGE_MONITOR_NEWS2_SENSOR_I2CDEV=y`,
   mantenha `BR2_PACKAGE_MONITOR_NEWS2_PPG_REFINED=y` e atualize o comentário acima delas.
2. **S30max30102** deixa de carregar o módulo `max30102`. Com o i2c-dev escolhido, o pacote já
   não instala o script (`monitor-news2.mk`, bloco `ifeq ($(BR2_PACKAGE_MONITOR_NEWS2_SENSOR_IIO),y)`).
   Sem udev/mdev, nada mais carrega o módulo. Confira na imagem nova que `/etc/init.d/S30max30102`
   não existe e que `lsmod` não lista `max30102`. Se o IIO for abandonado de vez, dá para remover
   o script, esse bloco do `.mk` e o `CONFIG_MAX30102=m` de `linux-news2.fragment`.
3. **config.txt** (`buildroot/external/board/rpi3-news2/config.txt`): remova a linha
   `dtoverlay=i2c-sensor,max30102,int_pin=17` e o bloco de comentário do MAX30102. Isso libera o
   **GPIO 17**. O i2c-dev não usa o pino INT, só `dtparam=i2c_arm=on`, que continua.
4. Ajuste o que menciona o IIO como padrão:
   - o `README.md` do `monitor-news2` (seção do sensor real);
   - o comentário do `rpi3_news2_defconfig`;
   - `docs/fluxos-do-sistema.md` (diagrama de aquisição).
5. Gere a imagem e grave o cartão:
   ```sh
   cd buildroot
   make
   make flash SD=/dev/sdX
   ```
   O `config.txt` vai para a partição de boot pelo `rpi-firmware`, então é preciso gerar a imagem
   inteira, não só o pacote.
6. Na imagem nova, repita a medição do passo 3 (sem o passo 2) e confira os critérios do passo 4.
