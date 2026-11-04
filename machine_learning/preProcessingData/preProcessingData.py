'''
Este script serve para realizar o pré-processamento de dados, para limpar e normalizar os dados a
serem utilizados pelos modelos de ML
'''

from pathlib import Path

import pandas as pd
import numpy as np
from sklearn.model_selection import GroupShuffleSplit

PATH = Path(__file__).resolve().parent.parent / "data" / "formatedData" / "patientsData.csv"
COLUMNS_TO_ANALYSE = ["Resp", "O2Sat", "SBP", "HR", "Temp", "FiO2"]
VALUES_TO_ACCEPT = {
    "Resp": (4, 60),
    "O2Sat": (70, 100),
    "SBP": (60, 270),
    "HR": (20, 250),
    "Temp": (25, 45),
    "FiO2": (0.21, 1.0),  #fração inspirada de O2: 0.21 = ar ambiente, 1.0 = O2 puro
}

FUTURE_WINDOW = 6  #janela deslizante prever se em alguma das próximas 6h, o paciente vai ser escore >=5
PAST_WINDOW = 6  #janela deslizante que obtém os sinais vitais e escore EWS das últimas 6h
THRESHOLD = 5  #NEWS2 >= 5 => "em risco" (métrica)
LAGS = (1, 2, 3)  #quantidade de 'fotos' que cada linha irá armazenar das variáveis anteriores para o modelo de ML
FIO2_FFILL_LIMIT = 4  #máximo de horas que o último registro de FiO2 é propagado; após isso, considera ar ambiente
ROOM_AIR_FIO2 = 0.21  #FiO2 do ar ambiente, usada quando não há registro de FiO2


class PreProcessing:
    def __init__(self):
        self.__feature_cols = None
        self.__dataFrame = self.cleanRegistersOutOfScope(self.getFormatedFileData())

        self.calculateNews2()

        self.generateTarget()

        self.buildFeatures()


    def getFormatedFileData(self):
        df = pd.read_csv(PATH, dtype={"patient_id": str, "source": str})
        dfOrdered = df.sort_values(["patient_id", "hour_idx"]).reset_index(drop=True)
        return dfOrdered

    def cleanRegistersOutOfScope(self, df):
        df = df.copy()

        df = self.applyPlausability(df)

        #Aplicar Forward-Fill
        #Forward-fill CAUSAL, por paciente, só propaga valores do passado para variáveis ausentes do futuro
        #FiO2 fica de fora aqui, pois tem forward-fill com limite (abaixo)
        vitals = [c for c in COLUMNS_TO_ANALYSE if c != "FiO2"]
        df[vitals] = df.groupby("patient_id")[vitals].ffill()

        #FiO2 é anotada de forma intermitente durante a oxigenoterapia: propaga o último registro
        #por até FIO2_FFILL_LIMIT horas. Sem registro recente => ar ambiente
        df["FiO2"] = df.groupby("patient_id")["FiO2"].ffill(limit=FIO2_FFILL_LIMIT)
        df["FiO2"] = df["FiO2"].fillna(ROOM_AIR_FIO2)

        #LIMPA
        #Descarta linhas que AINDA têm NaN em algum dos vitais (resíduo inicial, que não foi preenchido
        # para evitar data leackage)
        df = df.dropna(subset=COLUMNS_TO_ANALYSE).reset_index(drop=True)

        return df

    def applyPlausability(self, df):
        #remove valores que estão fora do possível científico, colocando como NaN.
        #posteriormente, será rodado um script de limpeza, que remove linhas que tem
        #variáveis NaN.
        df = df.copy()
        for v, (lo, hi) in VALUES_TO_ACCEPT.items():
            outRange = (df[v] < lo) | (df[v] > hi)
            df.loc[outRange, v] = np.nan
        return df

    def calculateNews2(self):
        #aplica cada valor do score baseado no news
        self.__dataFrame["s_resp"] = self.__dataFrame["Resp"].apply(self.score_resp)
        self.__dataFrame["s_spo2"] = self.__dataFrame["O2Sat"].apply(self.score_spo2)
        self.__dataFrame["s_sbp"] = self.__dataFrame["SBP"].apply(self.score_sbp)
        self.__dataFrame["s_hr"] = self.__dataFrame["HR"].apply(self.score_hr)
        self.__dataFrame["s_temp"] = self.__dataFrame["Temp"].apply(self.score_temp)
        self.__dataFrame["s_o2"] = self.__dataFrame["FiO2"].apply(self.score_o2)

        cols_score = ["s_resp", "s_spo2", "s_sbp", "s_hr", "s_temp", "s_o2"]
        self.__dataFrame["news2"] = self.__dataFrame[cols_score].sum(axis=1)

    def generateTarget(self, horizon=FUTURE_WINDOW, past_window=PAST_WINDOW, threshold=THRESHOLD):
        df = self.__dataFrame

        at_risk = (df["news2"] >= threshold).astype(int)

        # Y: olha para FRENTE (t+1 .. t+h), por paciente, sem cruzar fronteiras.
        # NaN quando não há `horizon` horas completas à frente (futuro incompleto).
        def future_window(s):
            arr = s.to_numpy()
            n = len(arr)
            out = np.full(n, np.nan)
            for i in range(n):
                if i + horizon <= n - 1:
                    out[i] = arr[i + 1 : i + horizon + 1].max()
            return pd.Series(out, index=s.index)

        df["_at_risk"] = at_risk
        df["Y"] = df.groupby("patient_id")["_at_risk"].transform(future_window)

        # elegível = tem passado suficiente (W) E futuro completo (Y não-NaN)
        position = df.groupby("patient_id").cumcount()
        df["eligible"] = (position >= past_window) & df["Y"].notna()

        df.drop(columns="_at_risk", inplace=True)
        self.__dataFrame = df

    #Função para criar as features das "fotos", para que o modelo de ML consiga ver a janela
    #deslizante passada.
    def buildFeatures(self, lags=LAGS, window=PAST_WINDOW):
        df = self.__dataFrame

        grp = df.groupby("patient_id")
        feature_cols = list(COLUMNS_TO_ANALYSE)  # o valor "atual" de cada vital é a própria coluna original

        for v in COLUMNS_TO_ANALYSE:
            # LAGS: valor em t-1, t-2, t-3 — shift dentro do groupby nunca cruza paciente nem vê o futuro
            for k in lags:
                col = f"{v}_lag{k}"
                df[col] = grp[v].shift(k)
                feature_cols.append(col)

            # DELTA: variação na janela (atual menos o valor de (window-1) horas atrás)
            col = f"{v}_delta"
            df[col] = df[v] - grp[v].shift(window - 1)
            feature_cols.append(col)

            # MÉDIA e DESVIO móveis — rolling é trailing (olha só p/ trás) => causal
            df[f"{v}_media"] = grp[v].transform(lambda s: s.rolling(window, min_periods=1).mean())
            df[f"{v}_std"] = grp[v].transform(lambda s: s.rolling(window, min_periods=1).std())
            feature_cols += [f"{v}_media", f"{v}_std"]

        # NEWS2 atual + variação recente: preditor forte e legítimo (só usa passado)
        df["news2_lag1"] = grp["news2"].shift(1)
        df["news2_delta"] = df["news2"] - grp["news2"].shift(window - 1)
        feature_cols += ["news2", "news2_lag1", "news2_delta"]

        self.__dataFrame = df
        self.__feature_cols = feature_cols
        print(f"Features criadas: {len(feature_cols)} colunas")
        return feature_cols

    def getTrainingMatrix(self):
        df = self.__dataFrame
        elig = df[df["eligible"]].copy()
        X = elig[self.__feature_cols]
        y = elig["Y"].astype(int)
        groups = elig["patient_id"]#guardado para o split POR PACIENTE
        return X, y, groups

    def splitByPatient(self, test_size=0.2, val_size=0.1, random_state=42):
        X, y, groups = self.getTrainingMatrix()

        #1º corte: separa o TESTE, usado apenas na avaliação final
        splitter = GroupShuffleSplit(n_splits=1, test_size=test_size,
                                     random_state=random_state)
        rest_idx, test_idx = next(splitter.split(X, y, groups))

        #2º corte: do restante, separa a VALIDAÇÃO (early stopping, limiar, hiperparâmetros)
        #val_size é relativo ao total, por isso é reescalado para a fração do restante
        splitter = GroupShuffleSplit(n_splits=1, test_size=val_size / (1 - test_size),
                                     random_state=random_state)
        tr, va = next(splitter.split(X.iloc[rest_idx], y.iloc[rest_idx], groups.iloc[rest_idx]))
        train_idx, val_idx = rest_idx[tr], rest_idx[va]

        X_train, X_val, X_test = X.iloc[train_idx], X.iloc[val_idx], X.iloc[test_idx]
        y_train, y_val, y_test = y.iloc[train_idx], y.iloc[val_idx], y.iloc[test_idx]

        print(f"Pacientes: treino={groups.iloc[train_idx].nunique()} | "
              f"validação={groups.iloc[val_idx].nunique()} | teste={groups.iloc[test_idx].nunique()}")

        return X_train, X_val, X_test, y_train, y_val, y_test

    def score_resp(self, rr):  # Frequência respiratória (irpm)
        if pd.isna(rr):
            return np.nan
        if rr <= 8:
            return 3
        if rr <= 11:
            return 1
        if rr <= 20:
            return 0
        if rr <= 24:
            return 2
        return 3

    def score_spo2(self, spo2):  # Saturação O2 — Escala 1 (%)
        if pd.isna(spo2):
            return np.nan
        if spo2 <= 91:
            return 3
        if spo2 <= 93:
            return 2
        if spo2 <= 95:
            return 1
        return 0

    def score_sbp(self, sbp):  # Pressão arterial sistólica (mmHg)
        if pd.isna(sbp):
            return np.nan
        if sbp <= 90:
            return 3
        if sbp <= 100:
            return 2
        if sbp <= 110:
            return 1
        if sbp <= 219:
            return 0
        return 3

    def score_hr(self, hr):  # Frequência cardíaca (bpm)
        if pd.isna(hr):
            return np.nan
        if hr <= 40:
            return 3
        if hr <= 50:
            return 1
        if hr <= 90:
            return 0
        if hr <= 110:
            return 1
        if hr <= 130:
            return 2
        return 3

    def score_temp(self, temp):  # Temperatura (°C)
        if pd.isna(temp):
            return np.nan
        if temp <= 35.0:
            return 1
        if temp <= 38.0:
            return 0
        if temp <= 39.0:
            return 1
        return 2

    def score_o2(self, fio2):  # O2 suplementar (FiO2 em fração)
        #sem registro de FiO2 (ou registro implausível, já anulado em applyPlausability)
        #=> considera que o paciente está respirando ar ambiente
        if pd.isna(fio2):
            return 0
        if fio2 > 0.21:
            return 2
        return 0
