import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.metrics import (average_precision_score, roc_auc_score,
                             precision_recall_curve, confusion_matrix,
                             classification_report, brier_score_loss)
from sklearn.model_selection import StratifiedGroupKFold

from machineLearningModels.evaluation import evaluateBySubset

# TODO (voltaremos nisso): o early stopping usa o conjunto de TESTE como eval_set (ver train()).
#  Isso faz o teste influenciar a escolha do nº de árvores => métricas levemente otimistas.
#  Correção: usar o conjunto de VALIDAÇÃO de splitByPatient() no eval_set e subir o teto de
#  n_estimators (com teto 2000, o early stopping escolheu ~560 árvores). Na validação cruzada,
#  usar uma validação interna por paciente em cada fold p/ o early stopping.

#hiperparâmetros obtidos no fine-tuning (FineTuning/RandomSearch.py) — usados no train() e na validação cruzada
XGB_PARAMS = dict(
    n_estimators=756,
    max_depth=4,
    learning_rate=0.038,
    subsample=0.63,
    colsample_bytree=0.61,
    gamma=4.21,
    min_child_weight=4,
    reg_alpha=0.24,
    reg_lambda=6.97,
    eval_metric="aucpr",  #otimiza p/ classe rara
    #early_stopping_rounds=30,
    n_jobs=-1,
    random_state=42,
)


class TrainModel:
    def __init__(self, X_train, X_test, y_train, y_test):
        self.model = None
        self.X_train, self.X_test = X_train, X_test
        self.y_train, self.y_test = y_train, y_test

        # desbalanceamento: razão negativos/positivos no TREINO
        n_pos = int(self.y_train.sum())
        n_neg = int(len(self.y_train) - n_pos)
        self.scale_pos_weight = n_neg / n_pos
        print(f"scale_pos_weight = {self.scale_pos_weight:.3f}  (neg/pos no treino)")

    def train(self):
        self.model = xgb.XGBClassifier(
            **XGB_PARAMS,
            scale_pos_weight=self.scale_pos_weight,  # compensa o desbalanceamento
        )
        self.model.fit(
            self.X_train, self.y_train,
            eval_set=[(self.X_test, self.y_test)],  # TODO: trocar pelo conjunto de validação
            verbose=False,
        )
        print(f"Árvores treinadas: {self.model.get_booster().num_boosted_rounds()}")
        return self.model

    def evaluate(self):
        # probabilidades (não o 0/1 cru) — é o que importa p/ early-warning
        proba = self.model.predict_proba(self.X_test)[:, 1]

        auprc = average_precision_score(self.y_test, proba)
        auroc = roc_auc_score(self.y_test, proba)
        brier = brier_score_loss(self.y_test, proba)
        baseline = self.y_test.mean()  # AUPRC de um chute aleatório

        print("\n=== Métricas (teste) ===")
        print(f"AUPRC:{auprc:.4f}(baseline aleatório = {baseline:.4f})")
        print(f"AUROC:{auroc:.4f}")
        print(f"Brier:{brier:.4f}(menor = melhor calibrado)")

        # limiar padrão 0.5 só para visualizar a matriz — NÃO é o limiar final
        pred = (proba >= 0.5).astype(int)
        print("\nMatriz de confusão (limiar 0.5):")
        print(confusion_matrix(self.y_test, pred))
        print("\n", classification_report(self.y_test, pred, digits=3))

        subsets = evaluateBySubset(proba, self.X_test, self.y_test)

        return {"auprc": auprc, "auroc": auroc, "brier": brier, **subsets}

    def featureImportance(self, n_repeats=5, sample_size=50000, top=15):
        '''
        Importância das variáveis por PERMUTAÇÃO no conjunto de teste: embaralha uma variável
        (ou um grupo) por vez e mede quanto a AUPRC cai. Quanto maior a queda, mais o modelo
        depende dela. Também mostra a importância nativa do XGBoost (gain), calculada no treino.
        '''
        X = self.X_test.sample(n=min(sample_size, len(self.X_test)), random_state=42)  #amostra p/ acelerar
        y = self.y_test.loc[X.index]
        rng = np.random.default_rng(42)
        base = average_precision_score(y, self.model.predict_proba(X)[:, 1])

        # POR VARIÁVEL
        rows = []
        for col in X.columns:
            mean, std = self.__permutationDrop(X, y, [col], base, rng, n_repeats)
            rows.append({"feature": col, "queda_auprc": mean, "desvio": std})
        by_feature = pd.DataFrame(rows)
        by_feature["gain_xgb"] = self.model.feature_importances_  #importância nativa (normalizada, soma 1)
        by_feature = by_feature.sort_values("queda_auprc", ascending=False).reset_index(drop=True)

        # POR GRUPO: o sinal vital inteiro (atual + lag1 + delta) é embaralhado junto,
        # pois variáveis correlacionadas (ex.: HR e HR_lag1) "cobrem" umas às outras na permutação individual
        groups = {}
        for col in X.columns:
            groups.setdefault(col.split("_")[0], []).append(col)
        rows = []
        for name, cols in groups.items():
            mean, std = self.__permutationDrop(X, y, cols, base, rng, n_repeats)
            rows.append({"grupo": name, "n_features": len(cols), "queda_auprc": mean, "desvio": std})
        by_group = pd.DataFrame(rows).sort_values("queda_auprc", ascending=False).reset_index(drop=True)

        print(f"\n=== Importância por permutação (teste, n={len(X)}, AUPRC base={base:.4f}) ===")
        print(f"\nTop {top} variáveis:")
        print(by_feature.head(top).to_string(index=False, float_format="%.4f"))
        print("\nPor sinal vital (grupo):")
        print(by_group.to_string(index=False, float_format="%.4f"))

        return by_feature, by_group

    def __permutationDrop(self, X, y, cols, base, rng, n_repeats):
        drops = []
        for _ in range(n_repeats):
            Xp = X.copy()
            #a mesma permutação p/ todas as colunas do grupo (mantém a relação entre elas)
            Xp[cols] = Xp[cols].to_numpy()[rng.permutation(len(Xp))]
            drops.append(base - average_precision_score(y, self.model.predict_proba(Xp)[:, 1]))
        return np.mean(drops), np.std(drops)

    def cross_validate_by_patient(self, X, y, groups, n_splits=5):
        cv = StratifiedGroupKFold(n_splits=n_splits, shuffle=True, random_state=42)
        auprc_scores, auroc_scores = [], []

        for fold, (tr_idx, te_idx) in enumerate(cv.split(X, y, groups), start=1):
            X_tr, X_te = X.iloc[tr_idx], X.iloc[te_idx]
            y_tr, y_te = y.iloc[tr_idx], y.iloc[te_idx]

            # checagem de vazamento POR FOLD (tem que dar 0)
            overlap = set(groups.iloc[tr_idx]) & set(groups.iloc[te_idx])
            assert len(overlap) == 0, f"Fold {fold}: {len(overlap)} pacientes vazando!"

            spw = (y_tr == 0).sum() / (y_tr == 1).sum()
            model = xgb.XGBClassifier(**XGB_PARAMS, scale_pos_weight=spw)
            model.fit(X_tr, y_tr)  # sem early stopping aqui, p/ não usar o fold de teste

            proba = model.predict_proba(X_te)[:, 1]
            ap = average_precision_score(y_te, proba)
            roc = roc_auc_score(y_te, proba)
            auprc_scores.append(ap)
            auroc_scores.append(roc)
            print(f"Fold {fold}: AUPRC={ap:.4f} | AUROC={roc:.4f}")

        print("\n=== Resumo (5 folds) ===")
        print(f"AUPRC: {np.mean(auprc_scores):.4f} ± {np.std(auprc_scores):.4f}")
        print(f"AUROC: {np.mean(auroc_scores):.4f} ± {np.std(auroc_scores):.4f}")
        return auprc_scores, auroc_scores
