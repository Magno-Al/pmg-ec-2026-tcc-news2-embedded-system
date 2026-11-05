# Machine Learning - Predição de deterioração pelo NEWS2

Modelos que preveem se o paciente vai atingir NEWS2 ≥ 5 em alguma das próximas 6 horas, a partir dos sinais vitais da hora atual e da anterior (Resp, O2Sat, SBP, HR, Temp, FiO2), medidos de 1 em 1 hora. São treinados XGBoost e Random Forest, com divisão de treino, validação e teste por paciente.

As 21 features usadas (valor atual, valor 1 h atrás e variação na última hora de cada sinal vital e do NEWS2) estão descritas em `features.json`. Além das métricas gerais, a avaliação compara o modelo com o próprio NEWS2 atual, separando os pacientes que já estão em risco (NEWS2 ≥ 5) dos que ainda não estão (NEWS2 < 5).

## Estrutura

```
machine_learning/
├── main.py                       # pré-processa, treina e avalia os modelos
├── features.json                 # lista e descrição das features usadas
├── preProcessingData/            # limpeza, cálculo do NEWS2, alvo e features
├── machineLearningModels/
│   ├── XgBoost.py
│   ├── RandomForest.py
│   ├── evaluation.py             # avaliação por subconjunto vs. NEWS2 atual
│   └── FineTuning/               # random search de hiperparâmetros e resultados
└── data/
    ├── raw/                      # dados brutos da PhysioNet (não versionados)
    └── formatedData/main.py      # junta os .psv em patientsData.csv
```

## Dados

Os dados vêm da base pública [PhysioNet/CinC Challenge 2019](https://physionet.org/content/challenge-2019/1.0.0/) e não estão no repositório: são cerca de 1,8 GB, e o CSV consolidado tem 155 MB, acima do limite do GitHub.

1. Baixe `training_setA` e `training_setB` e coloque em `data/raw/training/`.
2. Gere o CSV consolidado:

```
python data/formatedData/main.py
```

Isso cria `data/formatedData/patientsData.csv`, que é lido pelo pré-processamento.

## Como rodar

```
python -m venv .venv
.venv\Scripts\activate        # Windows (no Linux: source .venv/bin/activate)
pip install -r requirements.txt
python main.py
```

O ajuste de hiperparâmetros (random search) está comentado no fim do `main.py`, e os resultados ficam em `machineLearningModels/FineTuning/results/`.
