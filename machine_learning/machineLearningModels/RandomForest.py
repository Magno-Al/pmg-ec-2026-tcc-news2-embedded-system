import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import (average_precision_score, roc_auc_score,
                             confusion_matrix, classification_report,
                             brier_score_loss)
from sklearn.model_selection import StratifiedGroupKFold

RF_PARAMS = dict(
    n_estimators=134,
    max_depth=22,  #limita o tamanho das árvores (memória/tempo e embarque no dispositivo)
    min_samples_leaf=17,  #folhas com poucas amostras tendem a decorar ruído
    max_features=0.3,  #nº de features sorteadas a cada divisão
    class_weight="balanced",  #compensa o desbalanceamento (equivalente ao scale_pos_weight)
    n_jobs=-1,
    random_state=42,
    max_samples=0.44
)


class TrainModel:
    def __init__(self, X_train, X_val, X_test, y_train, y_val, y_test):
        self.model = None
        self.X_train, self.X_val, self.X_test = X_train, X_val, X_test
        self.y_train, self.y_val, self.y_test = y_train, y_val, y_test

        # desbalanceamento: razão negativos/positivos no TREINO (tratado via class_weight)
        n_pos = int(self.y_train.sum())
        n_neg = int(len(self.y_train) - n_pos)
        print(f"neg/pos no treino = {n_neg / n_pos:.3f}  (class_weight='balanced')")

    def train(self):
        self.model = RandomForestClassifier(**RF_PARAMS)
        self.model.fit(self.X_train, self.y_train)

        #Random Forest não usa early stopping: a VALIDAÇÃO serve só de acompanhamento
        #(e, futuramente, p/ ajuste de hiperparâmetros e limiar) — o teste não participa
        proba_val = self.model.predict_proba(self.X_val)[:, 1]
        print(f"AUPRC validação: {average_precision_score(self.y_val, proba_val):.4f}")
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

        return {"auprc": auprc, "auroc": auroc, "brier": brier}

    def featureImportance(self, n_repeats=5, sample_size=50000, top=15):
        '''
        Importância das variáveis por PERMUTAÇÃO no conjunto de teste: embaralha uma variável
        (ou um grupo) por vez e mede quanto a AUPRC cai. Quanto maior a queda, mais o modelo
        depende dela. Também mostra a importância nativa do Random Forest (MDI), calculada no treino.
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
        by_feature["mdi_rf"] = self.model.feature_importances_  #importância nativa (normalizada, soma 1)
        by_feature = by_feature.sort_values("queda_auprc", ascending=False).reset_index(drop=True)

        # POR GRUPO: o sinal vital inteiro (atual + lags + delta + média + std) é embaralhado junto,
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

            # sem early stopping no RF, então não precisa de validação interna
            model = RandomForestClassifier(**RF_PARAMS)
            model.fit(X_tr, y_tr)

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
