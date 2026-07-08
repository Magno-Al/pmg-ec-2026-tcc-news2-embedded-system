import numpy as np
import xgboost as xgb
from sklearn.metrics import (average_precision_score, roc_auc_score,
                             precision_recall_curve, confusion_matrix,
                             classification_report, brier_score_loss)
from sklearn.model_selection import StratifiedGroupKFold


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
            n_estimators=400,
            max_depth=5,
            learning_rate=0.05,
            subsample=0.8,
            colsample_bytree=0.8,
            scale_pos_weight=self.scale_pos_weight,  # compensa o 70/30
            eval_metric="aucpr",  #otimiza p/ classe rara
            early_stopping_rounds=30,
            n_jobs=-1,
            random_state=42,
        )
        self.model.fit(
            self.X_train, self.y_train,
            eval_set=[(self.X_test, self.y_test)],
            verbose=False,
        )
        print(f"Melhor iteração: {self.model.best_iteration}")
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
            model = xgb.XGBClassifier(
                n_estimators=400, max_depth=5, learning_rate=0.05,
                subsample=0.8, colsample_bytree=0.8,
                scale_pos_weight=spw, eval_metric="aucpr",
                n_jobs=-1, random_state=42,
            )
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
