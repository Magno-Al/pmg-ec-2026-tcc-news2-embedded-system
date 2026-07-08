# Sistema Embarcado para Monitoramento e Predição de Deterioração Clínica Intra-Hospitalar
A deterioração clínica de pacientes internados em enfermarias é frequentemente detectada
de forma tardia, em razão do monitoramento intermitente dos sinais vitais. Este trabalho
propõe um sistema embarcado de beira de leito que monitora continuamente os sinais vi-
tais, calcula o escore de alerta precoce NEWS2 na borda e prediz sua evolução por meio
de aprendizado de máquina. O dispositivo é construído sobre uma Raspberry Pi 3B+ com
sistema operacional Linux customizado gerado pelo Buildroot, integrando sensores de fo-
topletismografia e de temperatura, interface gráfica em Qt e periféricos de alerta. Para a
predição, são comparados três modelos de vieses indutivos distintos (regressão logística,
XGBoost e LSTM), treinados sobre uma base pública de pacientes hospitalizados. Como
resultados preliminares, o hardware foi definido e adquirido, os pacotes do sistema operaci-
onal embarcado foram definidos no Buildroot e a base de dados que fundamenta o modelo
preditivo foi normalizada e preparada para o treinamento. Os avanços confirmam a vi-
abilidade da solução, cuja finalização e validação serão conduzidas na etapa seguinte do
trabalho.

## Organização do repositório
O repositório está organizado, essencialmente, em 4 pastas: 
- buildroot: To-do
- firmware: To-do
- hardware: To-do
- ml: Modelos de Machine Learning, para cálculo e previsão da deterioração clínica do paciente

## Buildroot
- To-do
## Firmware
- To-do
## Hardware
- To-do
## ML
A metodologia parte de uma premissa central: em vez de classificar diretamente se o paciente terá ou não sepse, o modelo é responsável por **prever a evolução de valores fisiológicos** (como saturação de oxigênio e oxigenação sanguínea) e, a partir dessas previsões, calcular um **escore de alerta precoce**.

O escore escolhido é o **NEWS 2** (*National Early Warning Score 2*), amplamente utilizado e validado na prática clínica. Assim, o fluxo do trabalho se organiza em três etapas:

1. **Previsão dos sinais vitais** — o modelo de *Machine Learning* estima os valores fisiológicos futuros do paciente a partir de seus dados clínicos ao longo do tempo.
2. **Cálculo do NEWS 2** — os valores previstos são convertidos no escore NEWS 2, que atribui uma pontuação de risco com base em faixas de normalidade de cada parâmetro.
3. **Avaliação do quadro clínico** — o escore resultante indica o nível de deterioração e o risco de piora do paciente.

A opção por esse caminho, em vez de uma predição direta do desfecho, se justifica pela **interpretabilidade clínica**: o NEWS 2 é uma ferramenta reconhecida pela equipe de saúde, o que aproxima o resultado do modelo da realidade assistencial e facilita sua eventual adoção.


### Abordagem metodológica

A metodologia parte de uma premissa central: em vez de classificar diretamente se o paciente terá ou não sepse, o modelo é responsável por **prever a evolução de valores fisiológicos** (como saturação de oxigênio e oxigenação sanguínea) e, a partir dessas previsões, calcular um **escore de alerta precoce**.

O escore escolhido é o **NEWS 2** (*National Early Warning Score 2*), amplamente utilizado e validado na prática clínica. Assim, o fluxo do trabalho se organiza em três etapas:

1. **Previsão dos sinais vitais** — o modelo de *Machine Learning* estima os valores fisiológicos futuros do paciente a partir de seus dados clínicos ao longo do tempo.
2. **Cálculo do NEWS 2** — os valores previstos são convertidos no escore NEWS 2, que atribui uma pontuação de risco com base em faixas de normalidade de cada parâmetro.
3. **Avaliação do quadro clínico** — o escore resultante indica o nível de deterioração e o risco de piora do paciente.

A opção por esse caminho, em vez de uma predição direta do desfecho, se justifica pela **interpretabilidade clínica**: o NEWS 2 é uma ferramenta reconhecida pela equipe de saúde, o que aproxima o resultado do modelo da realidade assistencial e facilita sua eventual adoção.

### Base de dados

Os dados utilizados são provenientes do **PhysioNet/Computing in Cardiology Challenge 2019**, um conjunto público voltado à predição precoce de sepse. A base reúne registros de pacientes de UTI de dois hospitais — o **Beth Israel Deaconess Medical Center** e o **Emory University Hospital** —, contendo sinais vitais, exames laboratoriais e informações demográficas organizados hora a hora.

A rotulagem dos pacientes segue os critérios **Sepsis-3**, o consenso clínico atual que define a sepse a partir da disfunção orgânica associada a uma infecção. Esse rótulo indica o momento estimado de início da sepse, permitindo treinar o modelo para antecipá-lo.

### Considerações metodológicas

Alguns pontos importantes orientam e delimitam o trabalho:

- **Adequação da base ao score.** A maioria dos parâmetros exigidos pelo NEWS 2 está disponível na base de dados, o que a torna adequada ao objetivo. No entanto, o **nível de consciência** — um dos componentes do escore — não é registrado, o que representa uma limitação a ser tratada explicitamente.

- **Natureza dos dados.** Por serem registros clínicos reais, os dados apresentam grande quantidade de valores ausentes, especialmente nos exames laboratoriais. O tratamento dessas lacunas é parte essencial da metodologia.

- **Diferença entre sepse e deterioração geral.** A base é rotulada especificamente para sepse, enquanto o NEWS 2 avalia a deterioração clínica de forma mais ampla. O trabalho considera a sepse como um caso particular de deterioração, o que fundamenta a integração entre ambos.

- **Capacidade de generalização.** Modelos treinados em determinados hospitais tendem a apresentar desempenho inferior quando aplicados a instituições diferentes. Esse aspecto é levado em conta na avaliação, buscando resultados que reflitam de forma honesta a aplicabilidade do modelo.


