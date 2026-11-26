import numpy as np
import pandas as pd
from sklearn.calibration import CalibratedClassifierCV
from sklearn.frozen import FrozenEstimator
from sklearn.kernel_approximation import Nystroem
from sklearn.metrics import (accuracy_score, average_precision_score, roc_auc_score,
                             brier_score_loss)
from sklearn.model_selection import StratifiedGroupKFold
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.svm import LinearSVC

from machineLearningModels.evaluation import (evaluateBySubset, chooseThreshold,
                                              confusionMatrixReport)

#SVC com kernel exato escala entre O(n²) e O(n³) e é inviável nas ~790 mil linhas de treino.
#Por isso o kernel RBF é aproximado (Nyström) e a margem é otimizada por um SVM linear.
SVM_PARAMS = dict(
    gamma=0.01,  #alcance do kernel RBF
    C=1.0,  #inverso da regularização
    n_components=300,  #pontos de referência do Nyström (define memória e custo da predição)
)
#o Nyström gera uma matriz densa de n_components colunas: com 300 colunas, treinar com as
#789 mil linhas exigiria bem mais memória do que há disponível
MAX_TRAIN_SAMPLES = 236714  #30% do treino, a mesma fração usada no fine-tuning
RANDOM_STATE = 42


def buildSVM(gamma, C, n_components=SVM_PARAMS["n_components"]):
    return make_pipeline(
        StandardScaler(),  #SVM depende de distâncias: as variáveis precisam estar na mesma escala
        Nystroem(kernel="rbf", gamma=gamma, n_components=n_components, random_state=RANDOM_STATE),
        LinearSVC(C=C, dual=False, class_weight="balanced",  #compensa o desbalanceamento
                  max_iter=2000, random_state=RANDOM_STATE),
    )


def subsample(X, y, max_samples=MAX_TRAIN_SAMPLES):
    if len(X) <= max_samples:
        return X, y
    idx = X.sample(n=max_samples, random_state=RANDOM_STATE).index
    return X.loc[idx], y.loc[idx]


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
        X_fit, y_fit = subsample(self.X_train, self.y_train)
        print(f"Linhas usadas no treino do SVM: {len(X_fit)} de {len(self.X_train)}")
        svm = buildSVM(**SVM_PARAMS)
        svm.fit(X_fit, y_fit)

        #SVM devolve distância à margem, não probabilidade: a calibração de Platt converte
        #essa distância usando a VALIDAÇÃO — o teste não participa
        self.model = CalibratedClassifierCV(FrozenEstimator(svm), method="sigmoid")
        self.model.fit(self.X_val, self.y_val)

        proba_val = self.model.predict_proba(self.X_val)[:, 1]
        print(f"AUPRC validação: {average_precision_score(self.y_val, proba_val):.4f}")
        return self.model

    def evaluate(self, threshold=None):
        # probabilidades (não o 0/1 cru) — é o que importa p/ early-warning
        proba = self.model.predict_proba(self.X_test)[:, 1]

        auprc = average_precision_score(self.y_test, proba)
        auroc = roc_auc_score(self.y_test, proba)
        brier = brier_score_loss(self.y_test, proba)
        baseline = self.y_test.mean()  # AUPRC de um chute aleatório

        # limiar escolhido na VALIDAÇÃO (sensibilidade-alvo), nunca no teste
        if threshold is None:
            threshold = chooseThreshold(self.model.predict_proba(self.X_val)[:, 1], self.y_val)
        accuracy = accuracy_score(self.y_test, (proba >= threshold).astype(int))

        print("\n=== Métricas (teste) ===")
        print(f"AUPRC:{auprc:.4f}(baseline aleatório = {baseline:.4f})")
        print(f"AUROC:{auroc:.4f}")
        print(f"Brier:{brier:.4f}(menor = melhor calibrado)")
        print(f"Acurácia:{accuracy:.4f}(limiar {threshold:.4f})")

        cm_table = confusionMatrixReport(proba, self.X_test, self.y_test, threshold, "SVM")

        subsets = evaluateBySubset(proba, self.X_test, self.y_test)

        return {"auprc": auprc, "auroc": auroc, "brier": brier, "acuracia": accuracy,
                "limiar": threshold, "matriz_confusao": cm_table, **subsets}

    def featureImportance(self, n_repeats=5, sample_size=50000, top=15):
        '''
        Importância das variáveis por PERMUTAÇÃO no conjunto de teste: embaralha uma variável
        (ou um grupo) por vez e mede quanto a AUPRC cai. Quanto maior a queda, mais o modelo
        depende dela. Não há importância nativa aqui: os coeficientes do SVM estão no espaço
        do Nyström e não correspondem às variáveis originais.
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
        by_feature = pd.DataFrame(rows).sort_values("queda_auprc", ascending=False).reset_index(drop=True)

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

            X_fit, y_fit = subsample(X_tr, y_tr)
            model = buildSVM(**SVM_PARAMS)
            model.fit(X_fit, y_fit)

            # AUPRC e AUROC só dependem da ordenação: a distância à margem basta, sem calibração
            score = model.decision_function(X_te)
            ap = average_precision_score(y_te, score)
            roc = roc_auc_score(y_te, score)
            auprc_scores.append(ap)
            auroc_scores.append(roc)
            print(f"Fold {fold}: AUPRC={ap:.4f} | AUROC={roc:.4f}")

        print("\n=== Resumo (5 folds) ===")
        print(f"AUPRC: {np.mean(auprc_scores):.4f} ± {np.std(auprc_scores):.4f}")
        print(f"AUROC: {np.mean(auroc_scores):.4f} ± {np.std(auroc_scores):.4f}")
        return auprc_scores, auroc_scores
