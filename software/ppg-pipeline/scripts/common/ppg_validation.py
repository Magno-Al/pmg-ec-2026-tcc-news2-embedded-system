"""Lógica de validação de parâmetros fisiológicos (FC, SpO2, FR, ...) — compartilhada
por validate_heart_rate.py, validate_spo2.py e validate_respiratory_rate.py (ver
README, seção 6). Ferramenta offline (Python puro), fora do pipeline C++.

Cada janela cai em exatamente uma categoria: "comparavel" (status "aceita", com
valor calculado e referência no intervalo), "descartada" (reprovada no controle de
qualidade), "sem_calculo" (qualquer status diferente de "aceita", incluindo os
status específicos da FR fundida: "artefato_periodo_pulso"/"estimativa_ausente"/
"discordancia") ou "sem_referencia" ("aceita" com valor, mas sem referência no
intervalo).
"""

from pathlib import Path

import numpy as np
import pandas as pd
from scipy.stats import spearmanr


def resolve_start_time(value):
    """Aceita uma data/hora diretamente, ou o caminho de um CSV com coluna
    timestamp_abs (usa o timestamp da primeira linha — é assim que o CSV de PPG
    preparado marca o início do registro)."""
    try:
        return pd.to_datetime(value)
    except Exception:
        pass

    path = Path(value)
    if not path.exists():
        raise ValueError(f"--start-time não é uma data válida nem um arquivo existente: {value}")

    first_row = pd.read_csv(path, usecols=["timestamp_abs"], nrows=1)
    return pd.to_datetime(first_row["timestamp_abs"].iloc[0])


def derive_prefix(reference_csv_path, param_name):
    """<id>_reference_prepared.csv -> <id>_<param_name> (ex.: <id>_hr, <id>_spo2)."""
    reference_name = Path(reference_csv_path).stem
    if reference_name.endswith("_reference_prepared"):
        encounter_id = reference_name[: -len("_reference_prepared")]
    else:
        encounter_id = reference_name
    return f"{encounter_id}_{param_name}"


def _load_windows(params_csv_path, start_time, window_duration_s=None):
    """window_duration_s, quando passado, recalcula o início da janela a partir do
    fim (end_abs - window_duration_s) em vez de usar start_time_s direto -- usado
    pela FR, cuja janela real (60s) é maior que a exportada no CSV (30s)."""
    df = pd.read_csv(params_csv_path)
    df["end_abs"] = start_time + pd.to_timedelta(df["end_time_s"], unit="s")
    if window_duration_s is None:
        df["start_abs"] = start_time + pd.to_timedelta(df["start_time_s"], unit="s")
    else:
        df["start_abs"] = df["end_abs"] - pd.to_timedelta(window_duration_s, unit="s")
    return df


def _load_reference(reference_csv_path, ref_column):
    df = pd.read_csv(reference_csv_path, usecols=["timestamp_abs", ref_column])
    df["timestamp_abs"] = pd.to_datetime(df["timestamp_abs"])
    return df.sort_values("timestamp_abs").reset_index(drop=True)


def classify_windows(windows_df, reference_df, *, value_column, status_column, ref_column,
                      extra_count_column=None):
    ref_times = reference_df["timestamp_abs"].to_numpy()
    ref_values = reference_df[ref_column].to_numpy()

    categories, values_calculated, extra_counts = [], [], []
    values_reference_median, n_reference_samples, errors = [], [], []

    for row in windows_df.itertuples():
        status = getattr(row, status_column)
        value = getattr(row, value_column)
        if extra_count_column is not None:
            extra_counts.append(getattr(row, extra_count_column))

        if status == "descartada":
            categories.append("descartada")
            values_calculated.append(np.nan)
            values_reference_median.append(np.nan)
            n_reference_samples.append(np.nan)
            errors.append(np.nan)
            continue

        if status != "aceita" or pd.isna(value):
            categories.append("sem_calculo")
            values_calculated.append(value if not pd.isna(value) else np.nan)
            values_reference_median.append(np.nan)
            n_reference_samples.append(np.nan)
            errors.append(np.nan)
            continue

        in_window = (ref_times >= row.start_abs.to_datetime64()) & (ref_times <= row.end_abs.to_datetime64())
        matched = ref_values[in_window]
        matched = matched[~pd.isna(matched)]

        if len(matched) == 0:
            categories.append("sem_referencia")
            values_calculated.append(value)
            values_reference_median.append(np.nan)
            n_reference_samples.append(0)
            errors.append(np.nan)
            continue

        reference_value = float(np.median(matched))
        categories.append("comparavel")
        values_calculated.append(value)
        values_reference_median.append(reference_value)
        n_reference_samples.append(len(matched))
        errors.append(value - reference_value)

    columns = {
        "start_time_abs": windows_df["start_abs"],
        "end_time_abs": windows_df["end_abs"],
        "category": categories,
        "value_calculated": values_calculated,
    }
    if extra_count_column is not None:
        columns["n_calculated_samples"] = extra_counts
    columns["value_reference_median"] = values_reference_median
    columns["n_reference_samples"] = n_reference_samples
    columns["error"] = errors

    return pd.DataFrame(columns)


def summarize(detail_df):
    comparable = detail_df[detail_df["category"] == "comparavel"]
    n_total = len(detail_df)
    n_comparavel = len(comparable)

    if n_comparavel > 0:
        errors = comparable["error"].to_numpy()
        abs_errors = np.abs(errors)
        mae = float(np.mean(abs_errors))
        rmse = float(np.sqrt(np.mean(errors ** 2)))
        bias = float(np.mean(errors))
        std_error = float(np.std(errors, ddof=1)) if n_comparavel > 1 else np.nan
        pct_error_le_2 = float(100 * np.mean(abs_errors <= 2.0))
        pct_error_le_3 = float(100 * np.mean(abs_errors <= 3.0))
        if n_comparavel > 1:
            pearson_r = comparable["value_calculated"].corr(comparable["value_reference_median"])
            spearman_r, _ = spearmanr(comparable["value_calculated"], comparable["value_reference_median"])
        else:
            pearson_r = spearman_r = np.nan
    else:
        mae = rmse = bias = std_error = pearson_r = spearman_r = pct_error_le_2 = pct_error_le_3 = np.nan

    return pd.DataFrame([{
        "mae": mae,
        "rmse": rmse,
        "bias": bias,
        "std_error": std_error,
        "pearson_r": pearson_r,
        "spearman_r": spearman_r,
        "pct_error_le_2": pct_error_le_2,
        "pct_error_le_3": pct_error_le_3,
        "n_total": n_total,
        "n_comparavel": n_comparavel,
        "n_descartada": int((detail_df["category"] == "descartada").sum()),
        "n_sem_calculo": int((detail_df["category"] == "sem_calculo").sum()),
        "n_sem_referencia": int((detail_df["category"] == "sem_referencia").sum()),
        "coverage_pct": 100 * n_comparavel / n_total if n_total else np.nan,
    }])


def validate_parameter(params_csv_path, reference_csv_path, *, value_column, status_column,
                        ref_column, start_time, out_dir, out_prefix,
                        extra_count_column=None, window_duration_s=None):
    """Carrega os CSVs, classifica janela a janela, resume em métricas e grava os
    dois CSVs de saída. Retorna (detail_df, summary_df, detail_path, summary_path).
    window_duration_s: ver _load_windows."""
    windows_df = _load_windows(params_csv_path, start_time, window_duration_s)
    reference_df = _load_reference(reference_csv_path, ref_column)

    detail_df = classify_windows(
        windows_df, reference_df,
        value_column=value_column, status_column=status_column, ref_column=ref_column,
        extra_count_column=extra_count_column,
    )
    summary_df = summarize(detail_df)

    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    detail_path = out_dir / f"{out_prefix}_validation_detail.csv"
    summary_path = out_dir / f"{out_prefix}_validation_summary.csv"
    detail_df.to_csv(detail_path, index=False)
    summary_df.to_csv(summary_path, index=False)

    return detail_df, summary_df, detail_path, summary_path
