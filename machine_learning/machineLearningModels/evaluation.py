'''
Avaliação por subconjunto, comparando o modelo com o próprio NEWS2(t) como baseline.

Cerca de 42% dos positivos já estão em risco em t (NEWS2(t) >= 5): nesses casos, prever Y=1 é
quase trivial (persistência). O subconjunto que mostra o valor real do modelo é o de PIORA,
NEWS2(t) < 5, em que o paciente ainda não está em risco e o modelo precisa antecipar a deterioração.
'''

import numpy as np
import pandas as pd
from sklearn.metrics import average_precision_score, roc_auc_score, precision_recall_curve

from preProcessingData.preProcessingData import THRESHOLD


def evaluateBySubset(proba, X_test, y_test, threshold=THRESHOLD):
    news2 = X_test["news2"].to_numpy()
    y = y_test.to_numpy()
    proba = np.asarray(proba)

    subsets = {
        "Geral": np.ones(len(y), dtype=bool),
        f"Já em risco (NEWS2(t) >= {threshold})": news2 >= threshold,
        f"Piora (NEWS2(t) < {threshold})": news2 < threshold,
    }

    rows = []
    for name, m in subsets.items():
        yi, pi, ni = y[m], proba[m], news2[m]
        row = {"subconjunto": name, "n": int(m.sum()), "prevalencia": yi.mean()}
        if 0 < yi.sum() < len(yi):  # métricas só existem com as duas classes presentes
            row.update({
                "auprc_modelo": average_precision_score(yi, pi),
                "auprc_news2": average_precision_score(yi, ni),  # NEWS2(t) usado como score de risco
                "auroc_modelo": roc_auc_score(yi, pi),
                "auroc_news2": roc_auc_score(yi, ni),
            })
        rows.append(row)
    table = pd.DataFrame(rows)

    print("\n=== Modelo vs. NEWS2(t) como score, por subconjunto (teste) ===")
    print("(prevalência = AUPRC de um chute aleatório naquele subconjunto)")
    print(table.to_string(index=False, float_format="%.4f"))

    # Regra binária "alarme se NEWS2(t) >= limiar" vs. modelo operando no MESMO recall
    rule = news2 >= threshold
    tp = (rule & (y == 1)).sum()
    rule_recall = tp / (y == 1).sum()
    rule_precision = tp / rule.sum()
    prec, rec, _ = precision_recall_curve(y, proba)
    model_precision = prec[rec >= rule_recall].max()
    print(f"\nRegra NEWS2(t) >= {threshold}: recall={rule_recall:.4f} | precisão={rule_precision:.4f}")
    print(f"Modelo no mesmo recall:      recall={rule_recall:.4f} | precisão={model_precision:.4f}")

    return {
        "por_subconjunto": table,
        "regra": {"recall": rule_recall, "precisao": rule_precision},
        "modelo_mesmo_recall": {"recall": rule_recall, "precisao": model_precision},
    }
