'''
Este script realiza o fine-tuning dos hiperparâmetros do XGBoost e do Random Forest via Random Search.
Cada combinação sorteada é treinada no TREINO e avaliada (AUPRC) na VALIDAÇÃO.
O TESTE não participa: ele só é usado depois, na avaliação final com os melhores hiperparâmetros.
'''

import time
from pathlib import Path

import pandas as pd
import xgboost as xgb
from scipy.stats import randint, uniform, loguniform
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import average_precision_score, roc_auc_score
from sklearn.model_selection import ParameterSampler

N_ITER = 30  #nº de combinações sorteadas por modelo (mesmo orçamento p/ os dois => comparação justa)
SAMPLE_FRAC = 0.3  #fração das linhas de treino usada na busca (acelera; o modelo final usa 100%)
RANDOM_STATE = 42
RESULTS_DIR = Path(__file__).resolve().parent / "results"  #onde os CSVs com todas as tentativas são salvos

#espaço de busca do XGBoost (n_estimators não entra: quem decide é o early stopping)
XGB_SEARCH_SPACE = {
    "max_depth": randint(3, 11),            #3..10
    "min_child_weight": randint(1, 21),     #1..20
    "learning_rate": loguniform(0.01, 0.2),
    "subsample": uniform(0.6, 0.4),         #0.6..1.0
    "colsample_bytree": uniform(0.6, 0.4),  #0.6..1.0
    "gamma": uniform(0, 5),                 #ganho mínimo p/ dividir um nó
    "reg_lambda": loguniform(0.1, 10),      #regularização L2
    "reg_alpha": loguniform(1e-3, 10),      #regularização L1
}
XGB_FIXED = dict(
    n_estimators=2000,  #teto alto: o early stopping (na validação) escolhe o nº de árvores
    early_stopping_rounds=30,
    eval_metric="aucpr",
    tree_method="hist",
    n_jobs=-1,
    random_state=RANDOM_STATE,
)

#espaço de busca do Random Forest
RF_SEARCH_SPACE = {
    "n_estimators": randint(100, 401),
    "max_depth": [10, 14, 18, 22],  #sem None: árvores ilimitadas ficam enormes (inviável no Raspberry Pi)
    "min_samples_leaf": randint(10, 201),
    "max_features": ["sqrt", "log2", 0.3, 0.5],
    "max_samples": uniform(0.2, 0.4),  #0.2..0.6 — fração das linhas sorteada para cada árvore
    "class_weight": ["balanced", "balanced_subsample"],
}
RF_FIXED = dict(
    n_jobs=-1,
    random_state=RANDOM_STATE,
)

METRIC_COLUMNS = ["auprc_val", "auroc_val", "tempo_s"]


class RandomSearch:
    def __init__(self, X_train, X_val, y_train, y_val, sample_frac=SAMPLE_FRAC):
        #amostra das linhas de treino p/ acelerar a busca — a validação é usada completa
        if sample_frac < 1:
            idx = X_train.sample(frac=sample_frac, random_state=RANDOM_STATE).index
            X_train, y_train = X_train.loc[idx], y_train.loc[idx]

        self.X_train, self.X_val = X_train, X_val
        self.y_train, self.y_val = y_train, y_val

        # desbalanceamento: razão negativos/positivos no TREINO (usado pelo XGBoost)
        self.scale_pos_weight = (y_train == 0).sum() / (y_train == 1).sum()
        print(f"Random Search: {len(X_train)} linhas de treino | {len(X_val)} linhas de validação")

    def searchXgBoost(self, n_iter=N_ITER):
        def fit(params):
            model = xgb.XGBClassifier(**XGB_FIXED, **params, scale_pos_weight=self.scale_pos_weight)
            #early stopping na validação (a mesma usada p/ ranquear) — o teste segue intocado
            model.fit(self.X_train, self.y_train, eval_set=[(self.X_val, self.y_val)], verbose=False)
            return model, {"n_estimators": model.best_iteration + 1}

        return self.__search("XgBoost", XGB_SEARCH_SPACE, fit, n_iter)

    def searchRandomForest(self, n_iter=N_ITER):
        def fit(params):
            model = RandomForestClassifier(**RF_FIXED, **params)
            model.fit(self.X_train, self.y_train)
            return model, {}

        return self.__search("RandomForest", RF_SEARCH_SPACE, fit, n_iter)

    def __search(self, name, search_space, fit, n_iter):
        sampler = ParameterSampler(search_space, n_iter=n_iter, random_state=RANDOM_STATE)
        rows = []

        for i, params in enumerate(sampler, start=1):
            #np.float64/np.int64 -> float/int do Python (prints e CSV mais legíveis)
            params = {k: v.item() if hasattr(v, "item") else v for k, v in params.items()}
            start = time.time()
            model, extra = fit(params)

            proba = model.predict_proba(self.X_val)[:, 1]
            row = {
                **params,
                **extra,
                "auprc_val": average_precision_score(self.y_val, proba),
                "auroc_val": roc_auc_score(self.y_val, proba),
                "tempo_s": round(time.time() - start, 1),
            }
            rows.append(row)
            print(f"[{name} {i}/{n_iter}] AUPRC={row['auprc_val']:.4f} | AUROC={row['auroc_val']:.4f} "
                  f"| {row['tempo_s']}s | {params}")

        #ranqueia pela AUPRC na validação e salva todas as tentativas
        results = pd.DataFrame(rows).sort_values("auprc_val", ascending=False).reset_index(drop=True)
        RESULTS_DIR.mkdir(exist_ok=True)
        path = RESULTS_DIR / f"randomSearch_{name}.csv"
        results.to_csv(path, index=False)

        #melhor tentativa lida de `rows` (e não do DataFrame) p/ manter os tipos originais (int continua int)
        best = max(rows, key=lambda r: r["auprc_val"])
        best_params = {k: v for k, v in best.items() if k not in METRIC_COLUMNS}
        print(f"\n=== Melhor {name} (validação) ===")
        print(f"AUPRC: {best['auprc_val']:.4f} | AUROC: {best['auroc_val']:.4f}")
        print(f"Hiperparâmetros: {best_params}")
        print(f"Todas as tentativas salvas em: {path}")

        return best_params, results
