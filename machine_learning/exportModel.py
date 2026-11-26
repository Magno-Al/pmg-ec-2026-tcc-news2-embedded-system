'''
Treina o XGBoost final e exporta o que a Raspberry precisa para fazer a predição na borda:

  edge/model/xgb_news2.json  -> modelo no formato NATIVO do XGBoost (portável entre SO/arquitetura;
                                evita pickle/joblib, que quebram entre versões de bibliotecas)
  edge/model/metadata.json   -> ordem das features, limiar de alarme, faixas plausíveis etc.

Depois de exportar, roda um TESTE DE PARIDADE: reconstrói as leituras t e t-1 a partir do conjunto
de teste, passa pelo edge/news2Predictor.py e confere se a probabilidade é a mesma do modelo
original. Isso garante que o pré-processamento da borda é idêntico ao do treino.

Uso:  python exportModel.py
'''

import json
import sys
from datetime import datetime
from pathlib import Path

import numpy as np
import xgboost as xgb
from sklearn.metrics import average_precision_score, roc_auc_score

from preProcessingData.preProcessingData import (PreProcessing, VALUES_TO_ACCEPT, FUTURE_WINDOW,
                                                 THRESHOLD, FIO2_FFILL_LIMIT, ROOM_AIR_FIO2)
from machineLearningModels.XgBoost import TrainModel as XgBoost, XGB_PARAMS
from machineLearningModels.evaluation import chooseThreshold, TARGET_RECALL

EDGE_DIR = Path(__file__).resolve().parent / "edge"
MODEL_DIR = EDGE_DIR / "model"
MODEL_FILE = "xgb_news2.json"


def export():
    data = PreProcessing()
    X_train, X_val, X_test, y_train, y_val, y_test = data.splitByPatient()

    trainer = XgBoost(X_train, X_val, X_test, y_train, y_val, y_test)
    model = trainer.train()

    #limiar escolhido na VALIDAÇÃO (mesma regra usada no evaluate())
    threshold = float(chooseThreshold(model.predict_proba(X_val)[:, 1], y_val))
    proba_test = model.predict_proba(X_test)[:, 1]

    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    model.save_model(MODEL_DIR / MODEL_FILE)

    metadata = {
        "model_file": MODEL_FILE,
        "feature_names": list(X_train.columns),  #ORDEM importa: o vetor da borda segue esta lista
        "threshold": threshold,
        "target_recall": TARGET_RECALL,
        "horizon_hours": FUTURE_WINDOW,
        "news2_risk_threshold": THRESHOLD,
        "plausible_ranges": {k: list(v) for k, v in VALUES_TO_ACCEPT.items()},
        "room_air_fio2": ROOM_AIR_FIO2,
        "fio2_ffill_limit": FIO2_FFILL_LIMIT,
        "xgb_params": {k: v for k, v in XGB_PARAMS.items() if k != "n_jobs"},
        "scale_pos_weight": trainer.scale_pos_weight,
        "test_metrics": {
            "auprc": float(average_precision_score(y_test, proba_test)),
            "auroc": float(roc_auc_score(y_test, proba_test)),
        },
        "xgboost_version": xgb.__version__,
        "exported_at": datetime.now().isoformat(timespec="seconds"),
    }
    with open(MODEL_DIR / "metadata.json", "w", encoding="utf-8") as f:
        json.dump(metadata, f, indent=2, ensure_ascii=False)

    print(f"\nModelo salvo em: {MODEL_DIR / MODEL_FILE}")
    print(f"Metadados em:    {MODEL_DIR / 'metadata.json'}")
    print(f"Limiar de alarme (recall-alvo {TARGET_RECALL:.0%} na validação): {threshold:.4f}")

    parityCheck(X_test, proba_test)


def parityCheck(X_test, proba_test, n=2000):
    sys.path.insert(0, str(EDGE_DIR))
    from news2Predictor import News2Predictor, VITALS

    predictor = News2Predictor(MODEL_DIR)
    rng = np.random.default_rng(42)
    idx = rng.choice(len(X_test), size=min(n, len(X_test)), replace=False)

    diffs = []
    for i in idx:
        row = X_test.iloc[i]
        current = {v: row[v] for v in VITALS}
        previous = {v: row[f"{v}_lag1"] for v in VITALS}
        diffs.append(abs(predictor.predict(current, previous)["probabilidade"] - proba_test[i]))

    max_diff = max(diffs)
    print(f"\nTeste de paridade (borda vs. treino, {len(idx)} amostras): maior diferença = {max_diff:.2e}")
    assert max_diff < 1e-5, "Pré-processamento da borda diverge do treino!"
    print("OK: edge/news2Predictor.py reproduz o modelo original.")


if __name__ == "__main__":
    export()
