'''
Avaliação por subconjunto, comparando o modelo com o próprio NEWS2(t) como baseline.

Cerca de 42% dos positivos já estão em risco em t (NEWS2(t) >= 5): nesses casos, prever Y=1 é
quase trivial (persistência). O subconjunto que mostra o valor real do modelo é o de PIORA,
NEWS2(t) < 5, em que o paciente ainda não está em risco e o modelo precisa antecipar a deterioração.
'''

import os

import numpy as np
import pandas as pd
from sklearn.metrics import (average_precision_score, roc_auc_score, precision_recall_curve,
                             confusion_matrix)

from preProcessingData.preProcessingData import THRESHOLD

#sensibilidade-alvo usada p/ escolher o limiar de alarme na VALIDAÇÃO (o teste não participa)
TARGET_RECALL = 0.80
RESULTS_DIR = os.path.join(os.path.dirname(__file__), "results")


def _subsets(news2, threshold):
    return {
        "Geral": np.ones(len(news2), dtype=bool),
        f"Já em risco (NEWS2(t) >= {threshold})": news2 >= threshold,
        f"Piora (NEWS2(t) < {threshold})": news2 < threshold,
    }


def evaluateBySubset(proba, X_test, y_test, threshold=THRESHOLD):
    news2 = X_test["news2"].to_numpy()
    y = y_test.to_numpy()
    proba = np.asarray(proba)

    subsets = _subsets(news2, threshold)

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


def chooseThreshold(proba_val, y_val, target_recall=TARGET_RECALL):
    '''
    Maior limiar cuja sensibilidade na VALIDAÇÃO é >= target_recall. O limiar 0.5 não tem
    significado aqui: com class_weight/scale_pos_weight e calibração, as probabilidades ficam
    deslocadas, e em early-warning o que se fixa é quantos eventos o alarme pode deixar passar.
    '''
    _, rec, thr = precision_recall_curve(y_val, proba_val)
    t = thr[rec[:-1] >= target_recall].max()  #rec[i] corresponde a thr[i]; rec é decrescente
    print(f"\nLimiar escolhido na validação: {t:.4f} (sensibilidade-alvo = {target_recall:.0%})")
    return t


def _div(a, b):
    return a / b if b else np.nan


def confusionStats(y, pred):
    tn, fp, fn, tp = confusion_matrix(y, pred, labels=[0, 1]).ravel()
    precision = _div(tp, tp + fp)
    recall = _div(tp, tp + fn)
    return {
        "TN": tn, "FP": fp, "FN": fn, "TP": tp,
        "sensibilidade": recall,                 #recall: dos que pioraram, quantos o alarme pegou
        "especificidade": _div(tn, tn + fp),     #dos estáveis, quantos ficaram sem alarme
        "VPP": precision,                        #precisão: dos alarmes, quantos eram reais
        "VPN": _div(tn, tn + fn),
        "F1": _div(2 * precision * recall, precision + recall),
        "acuracia": _div(tp + tn, tp + tn + fp + fn),
        "taxa_alarme": _div(tp + fp, tp + tn + fp + fn),  #fração das janelas que disparam alarme
        "NNE": _div(tp + fp, tp),                #alarmes avaliados por evento real (= 1/VPP)
    }


def confusionMatrixReport(proba, X_test, y_test, threshold, model_name,
                          news2_threshold=THRESHOLD, save_dir=RESULTS_DIR):
    '''
    Matriz de confusão do modelo (no limiar dado) e da regra "alarme se NEWS2(t) >= 5",
    por subconjunto. Imprime a matriz geral rotulada, uma tabela com as métricas derivadas
    e salva a figura em save_dir (se o matplotlib estiver instalado).
    '''
    news2 = X_test["news2"].to_numpy()
    y = y_test.to_numpy()
    classifiers = {
        "Modelo": np.asarray(proba) >= threshold,
        f"Regra NEWS2(t) >= {news2_threshold}": news2 >= news2_threshold,
    }
    subsets = _subsets(news2, news2_threshold)

    rows, matrices = [], {}
    for sub_name, m in subsets.items():
        for clf_name, pred in classifiers.items():
            stats = confusionStats(y[m], pred[m].astype(int))
            rows.append({"subconjunto": sub_name, "classificador": clf_name, **stats})
            matrices[(clf_name, sub_name)] = np.array([[stats["TN"], stats["FP"]],
                                                       [stats["FN"], stats["TP"]]])
    table = pd.DataFrame(rows)

    labels_true = ["Real 0 (sem piora)", "Real 1 (NEWS2>=5 em 6h)"]
    labels_pred = ["Previsto 0", "Previsto 1"]
    print(f"\n=== Matriz de confusão — {model_name} (teste, limiar {threshold:.4f}) ===")
    print(pd.DataFrame(matrices[("Modelo", "Geral")], index=labels_true, columns=labels_pred))

    print("\nMétricas derivadas da matriz, por subconjunto:")
    cols = ["subconjunto", "classificador", "TP", "FP", "FN", "TN", "sensibilidade",
            "especificidade", "VPP", "VPN", "F1", "taxa_alarme", "NNE"]
    print(table[cols].to_string(index=False, float_format="%.4f"))

    if save_dir:
        _plotConfusion(matrices, list(classifiers), list(subsets), model_name, threshold, save_dir)
    return table


def _plotConfusion(matrices, clf_names, sub_names, model_name, threshold, save_dir):
    try:
        import matplotlib
        matplotlib.use("Agg")  #só salva o arquivo, não abre janela
        import matplotlib.pyplot as plt
    except ImportError:
        print("(matplotlib não instalado: figura da matriz de confusão não gerada)")
        return

    fig, axes = plt.subplots(len(clf_names), len(sub_names),
                             figsize=(4.2 * len(sub_names), 3.8 * len(clf_names)))
    for i, clf in enumerate(clf_names):
        for j, sub in enumerate(sub_names):
            ax, cm = axes[i, j], matrices[(clf, sub)]
            #normalizado por linha (classe real): a diagonal vira especificidade e sensibilidade
            row_sum = cm.sum(axis=1, keepdims=True)
            norm = np.divide(cm, row_sum, out=np.zeros_like(cm, dtype=float), where=row_sum > 0)
            ax.imshow(norm, cmap="Blues", vmin=0, vmax=1)
            for r in range(2):
                for c in range(2):
                    ax.text(c, r, f"{cm[r, c]:,}\n({norm[r, c]:.1%})", ha="center", va="center",
                            color="white" if norm[r, c] > 0.5 else "black", fontsize=10)
            ax.set_xticks([0, 1], ["Prev. 0", "Prev. 1"])
            ax.set_yticks([0, 1], ["Real 0", "Real 1"])
            ax.set_title(f"{clf}\n{sub}", fontsize=9)
    fig.suptitle(f"{model_name} — limiar {threshold:.4f} (teste)")
    fig.tight_layout()

    os.makedirs(save_dir, exist_ok=True)
    path = os.path.join(save_dir, f"matrizConfusao_{model_name}.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"Figura salva em: {path}")
