'''
Inferência do XGBoost na borda (Raspberry Pi).

Este arquivo é AUTOSSUFICIENTE: depende apenas de numpy e xgboost (sem pandas/sklearn e sem o
resto do projeto). Basta copiar a pasta `edge/` (com `model/`) para a Raspberry.

Ele reproduz exatamente o pré-processamento do treino (preProcessingData.py):
  1. NEWS2 calculado a partir dos 6 sinais vitais
  2. features: valor atual (t), lag1 (t-1) e delta (t - (t-1)) de cada vital e do NEWS2
  3. vetor montado na MESMA ORDEM de colunas usada no treino (lida de metadata.json)

Uso rápido:
    from news2Predictor import News2Predictor
    p = News2Predictor()
    p.predict(current={...}, previous={...})   # leituras de t e t-1

Ou, recebendo uma leitura por hora (o preditor guarda a leitura anterior):
    from news2Predictor import StreamingNews2Predictor
    s = StreamingNews2Predictor()
    s.update({...})   # 1ª leitura -> None (ainda não existe t-1)
    s.update({...})   # a partir da 2ª -> predição
'''

import json
from pathlib import Path

import numpy as np
import xgboost as xgb

MODEL_DIR = Path(__file__).resolve().parent / "model"
VITALS = ("Resp", "O2Sat", "SBP", "HR", "Temp", "FiO2")


# ---------------------------------------------------------------------------------------------
# NEWS2 — mesmas regras de preProcessingData.py (qualquer mudança lá precisa ser replicada aqui;
# o exportModel.py faz um teste de paridade que acusa divergência)
# ---------------------------------------------------------------------------------------------
def score_resp(rr):
    if rr <= 8: return 3
    if rr <= 11: return 1
    if rr <= 20: return 0
    if rr <= 24: return 2
    return 3


def score_spo2(spo2):
    if spo2 <= 91: return 3
    if spo2 <= 93: return 2
    if spo2 <= 95: return 1
    return 0


def score_sbp(sbp):
    if sbp <= 90: return 3
    if sbp <= 100: return 2
    if sbp <= 110: return 1
    if sbp <= 219: return 0
    return 3


def score_hr(hr):
    if hr <= 40: return 3
    if hr <= 50: return 1
    if hr <= 90: return 0
    if hr <= 110: return 1
    if hr <= 130: return 2
    return 3


def score_temp(temp):
    if temp <= 35.0: return 1
    if temp <= 38.0: return 0
    if temp <= 39.0: return 1
    return 2


def score_o2(fio2):
    return 2 if fio2 > 0.21 else 0


def news2(v):
    return (score_resp(v["Resp"]) + score_spo2(v["O2Sat"]) + score_sbp(v["SBP"])
            + score_hr(v["HR"]) + score_temp(v["Temp"]) + score_o2(v["FiO2"]))


# ---------------------------------------------------------------------------------------------
class News2Predictor:
    def __init__(self, model_dir=MODEL_DIR):
        model_dir = Path(model_dir)
        with open(model_dir / "metadata.json", encoding="utf-8") as f:
            self.meta = json.load(f)

        self.booster = xgb.Booster()
        self.booster.load_model(model_dir / self.meta["model_file"])

        self.feature_names = self.meta["feature_names"]
        self.threshold = self.meta["threshold"]
        self.ranges = self.meta["plausible_ranges"]
        self.room_air_fio2 = self.meta["room_air_fio2"]

    def _clean(self, reading, label):
        '''Valida uma leitura: FiO2 ausente => ar ambiente; demais vitais são obrigatórios e
        precisam estar dentro da faixa plausível usada no treino.'''
        v = {}
        for name in VITALS:
            value = reading.get(name)
            if value is None and name == "FiO2":
                value = self.room_air_fio2
            if value is None:
                raise ValueError(f"[{label}] sinal vital ausente: {name}")
            value = float(value)
            lo, hi = self.ranges[name]
            if not lo <= value <= hi:
                raise ValueError(f"[{label}] {name}={value} fora da faixa plausível [{lo}, {hi}]")
            v[name] = value
        return v

    def buildFeatures(self, current, previous):
        '''Monta o vetor de features (1 x n) na mesma ordem do treino.'''
        cur = self._clean(current, "t")
        prev = self._clean(previous, "t-1")
        cur["news2"], prev["news2"] = news2(cur), news2(prev)

        feats = {}
        for name in (*VITALS, "news2"):
            feats[name] = cur[name]
            feats[f"{name}_lag1"] = prev[name]
            feats[f"{name}_delta"] = cur[name] - prev[name]

        x = np.array([[feats[c] for c in self.feature_names]], dtype=np.float32)
        return x, cur["news2"]

    def predict(self, current, previous):
        '''
        current / previous: dicts com Resp, O2Sat, SBP, HR, Temp, FiO2 (FiO2 em fração 0.21–1.0)
        nas horas t e t-1. Retorna a probabilidade de NEWS2 >= 5 nas próximas horas e o alarme.
        '''
        x, news2_now = self.buildFeatures(current, previous)
        proba = float(self.booster.inplace_predict(x)[0])
        return {
            "probabilidade": proba,
            "alerta": proba >= self.threshold,
            "limiar": self.threshold,
            "news2_atual": news2_now,
            "horizonte_horas": self.meta["horizon_hours"],
        }


class StreamingNews2Predictor(News2Predictor):
    '''
    Versão com estado, para um paciente: recebe uma leitura por hora e guarda a anterior.
    Imita o forward-fill CAUSAL do treino: vital ausente na leitura => usa o último valor
    conhecido; FiO2 é propagada por até `fio2_ffill_limit` horas e depois vira ar ambiente.
    '''

    def __init__(self, model_dir=MODEL_DIR):
        super().__init__(model_dir)
        self.fio2_ffill_limit = self.meta["fio2_ffill_limit"]
        self.reset()

    def reset(self):
        self._last = {}        # último valor conhecido de cada vital
        self._fio2_age = 0     # horas desde o último registro de FiO2
        self._previous = None  # leitura completa de t-1

    def _fill(self, reading):
        filled = {}
        for name in VITALS:
            value = reading.get(name)
            if value is not None:
                lo, hi = self.ranges[name]
                if not lo <= float(value) <= hi:  # implausível => tratado como ausente (igual ao treino)
                    value = None
            if name == "FiO2":
                if value is not None:
                    self._fio2_age = 0
                else:
                    self._fio2_age += 1
                    if self._fio2_age > self.fio2_ffill_limit:
                        self._last.pop("FiO2", None)
                value = value if value is not None else self._last.get("FiO2", self.room_air_fio2)
            elif value is None:
                value = self._last.get(name)
            if value is not None:
                self._last[name] = float(value)
            filled[name] = value
        return filled

    def update(self, reading):
        '''Recebe a leitura da hora atual. Retorna None enquanto não houver t-1 completo.'''
        current = self._fill(reading)
        if any(current[n] is None for n in VITALS):
            return None  # ainda falta algum vital nunca medido (no treino essa linha era descartada)
        previous, self._previous = self._previous, current
        if previous is None:
            return None
        return self.predict(current, previous)


if __name__ == "__main__":
    predictor = News2Predictor()
    previous = {"Resp": 18, "O2Sat": 96, "SBP": 125, "HR": 88, "Temp": 37.2, "FiO2": 0.21}
    current = {"Resp": 23, "O2Sat": 93, "SBP": 108, "HR": 104, "Temp": 38.1, "FiO2": 0.21}
    print(predictor.predict(current, previous))
