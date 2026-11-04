"""Roda o pipeline C++ sobre cada registro preparado da BIDMC (ver
prepare_bidmc_data.py).

Fase 1 (--phase 1, default): diagnósticos SEM calcular erro nem métrica contra
a referência -- taxa de amostragem lida, escala do PPG, distribuição do Índice
de Perfusão e % de janelas aprovadas pelo controle de qualidade, contagem de
status de FC/FR, e confirmação de que as janelas exportadas cobrem 30s/60s com
hop de 5s. Não chama scripts/common/ppg_validation.py nesta fase.

Fase 2 (--phase 2): validação contra a referência, chamando
scripts/common/ppg_validation.py (núcleo de validação, inalterado) para FC e
para FR (valor fundido e as três modulações), em cada registro. Produz,
por registro, MAE/viés/cobertura/contagens de status (debug_output/bidmc/
<registro>/) e, agregado, duas formas de resumo em debug_output/bidmc/
aggregate/: mediana do MAE por registro com intervalo interquartil (formato
usado na literatura com a BIDMC), e métricas agrupando todas as janelas
comparáveis de todos os registros (MAE/RMSE/viés/Pearson/Spearman/%erro/
cobertura, via a mesma função summarize() do núcleo, não reimplementada aqui).

Fase 3 (--phase 3): valida a FR (fundida + três modulações) contra as
referências derivadas das anotações manuais de respiração (ver
prepare_bidmc_breath_reference.py), reaproveitando os ppg_parameters.csv JÁ
exportados pela Fase 2 -- o pipeline NÃO roda de novo. Grava em
debug_output/bidmc/aggregate_breath_annotations/ (subpastas por registro +
aggregate/), sem sobrescrever os resultados da Fase 2. Também compara as três
referências entre si (RESP do monitor, anotador 1, anotador 2) usando o mesmo
alinhamento de janela de 60s, via classify_windows com um status/valor
"dummy" só para extrair a mediana de referência por janela independente do
resultado do pipeline (ver reference_median_per_window).

Ferramenta offline (Python puro) — não faz parte do pipeline C++ embarcado.

Uso (rode a partir deste diretório, scripts/bidmc, ou ajuste os caminhos):
    python run_bidmc_validation.py [registro ...] --phase 1
    python run_bidmc_validation.py [registro ...] --phase 2
    python run_bidmc_validation.py [registro ...] --phase 3
"""

import argparse
import csv
import os
import statistics
import subprocess
import sys
from collections import Counter
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
from ppg_validation import (  # noqa: E402
    _load_windows, _load_reference, classify_windows, resolve_start_time, summarize, validate_parameter,
)

BREATH_ANNOTATORS = ("ann1", "ann2")

# Igual a PPG_WINDOW_SECONDS_MAX (include/PPG.hpp); ver validate_respiratory_rate.py.
RR_WINDOW_DURATION_S = 60.0

# Um parametro -> argumentos de validate_parameter(). "rr_fused" e o valor
# fundido pelo Smart Fusion; as outras tres sao as modulacoes individuais.
PARAMETERS = {
    "hr": dict(value_column="hr_value", status_column="hr_status", ref_column="hr_ref",
               extra_count_column="hr_peak_count", window_duration_s=None),
    "rr_fused": dict(value_column="rr_value", status_column="rr_status", ref_column="rr_ref",
                      extra_count_column=None, window_duration_s=RR_WINDOW_DURATION_S),
    "rr_amplitude": dict(value_column="rr_amplitude_value", status_column="rr_amplitude_status", ref_column="rr_ref",
                          extra_count_column=None, window_duration_s=RR_WINDOW_DURATION_S),
    "rr_frequency": dict(value_column="rr_frequency_value", status_column="rr_frequency_status", ref_column="rr_ref",
                          extra_count_column=None, window_duration_s=RR_WINDOW_DURATION_S),
    "rr_baseline": dict(value_column="rr_baseline_value", status_column="rr_baseline_status", ref_column="rr_ref",
                         extra_count_column=None, window_duration_s=RR_WINDOW_DURATION_S),
}


def find_prepared_records(prepared_dir):
    prepared_dir = Path(prepared_dir)
    return sorted(p.name.removesuffix("_ppg_prepared.csv") for p in prepared_dir.glob("*_ppg_prepared.csv"))


def read_meta_sample_rate(meta_path):
    for line in Path(meta_path).read_text().splitlines():
        if line.startswith("sample_rate_hz="):
            return int(line.split("=", 1)[1])
    return None


def run_pipeline(tcc_binary, mock_file, param_export_path):
    env = os.environ.copy()
    env["PPG_MOCK_FILE"] = str(mock_file)
    env["PPG_PARAM_EXPORT"] = "1"
    env["PPG_PARAM_EXPORT_PATH"] = str(param_export_path)
    return subprocess.run([str(tcc_binary)], env=env, capture_output=True, text=True)


def read_params_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def summarize_ppg_scale(ppg_csv_path):
    values = []
    with open(ppg_csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            values.append(float(row["ir_signal"]))
    return min(values), max(values), statistics.mean(values)


def pct(numerator, denominator):
    return 100.0 * numerator / denominator if denominator else float("nan")


def phase1_report(records, prepared_dir, out_dir, tcc_binary):
    prepared_dir = Path(prepared_dir)
    out_dir = Path(out_dir)

    per_record = []
    all_rows = []

    for record_id in records:
        ppg_csv = prepared_dir / f"{record_id}_ppg_prepared.csv"
        meta_path = prepared_dir / f"{record_id}_ppg_prepared.meta"
        record_out_dir = out_dir / record_id
        record_out_dir.mkdir(parents=True, exist_ok=True)
        params_path = record_out_dir / f"{record_id}_ppg_parameters.csv"

        result = run_pipeline(tcc_binary, ppg_csv, params_path)
        if result.returncode != 0:
            print(f"ERRO ao rodar o pipeline para {record_id} (codigo {result.returncode}):", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            continue

        sample_rate_hz = read_meta_sample_rate(meta_path)
        ppg_min, ppg_max, ppg_mean = summarize_ppg_scale(ppg_csv)
        rows = read_params_csv(params_path)
        all_rows.extend(rows)

        n_windows = len(rows)
        n_usable_hr = sum(1 for r in rows if r["hr_status"] != "descartada")
        n_usable_rr = sum(1 for r in rows if r["rr_status"] != "descartada")
        pi_ir_30s = [float(r["pi_ir_30s"]) for r in rows]
        pi_red_30s = [float(r["pi_red_30s"]) for r in rows]
        pi_ir_60s = [float(r["pi_ir_60s"]) for r in rows]

        per_record.append({
            "record_id": record_id,
            "sample_rate_hz": sample_rate_hz,
            "ppg_min": ppg_min, "ppg_max": ppg_max, "ppg_mean": ppg_mean,
            "n_windows": n_windows,
            "n_usable_hr": n_usable_hr, "n_usable_rr": n_usable_rr,
            "pi_ir_30s_mean": statistics.mean(pi_ir_30s) if pi_ir_30s else float("nan"),
            "pi_red_30s_mean": statistics.mean(pi_red_30s) if pi_red_30s else float("nan"),
            "pi_ir_60s_mean": statistics.mean(pi_ir_60s) if pi_ir_60s else float("nan"),
        })

        print(f"{record_id}: taxa={sample_rate_hz}Hz, ppg[min={ppg_min:.0f} max={ppg_max:.0f} "
              f"media={ppg_mean:.0f}], janelas={n_windows}, "
              f"usable_hr={n_usable_hr}/{n_windows} ({pct(n_usable_hr, n_windows):.1f}%), "
              f"usable_rr={n_usable_rr}/{n_windows} ({pct(n_usable_rr, n_windows):.1f}%), "
              f"PI medio ir_30s={statistics.mean(pi_ir_30s):.5f}")

    print()
    print("=== resumo agregado (todos os registros, todas as janelas) ===")
    total_windows = len(all_rows)
    total_usable_hr = sum(1 for r in all_rows if r["hr_status"] != "descartada")
    total_usable_rr = sum(1 for r in all_rows if r["rr_status"] != "descartada")
    print(f"janelas totais: {total_windows}")
    print(f"usable_for_hr: {total_usable_hr} ({pct(total_usable_hr, total_windows):.2f}%)")
    print(f"usable_for_rr: {total_usable_rr} ({pct(total_usable_rr, total_windows):.2f}%)")

    all_pi_ir_30s = [float(r["pi_ir_30s"]) for r in all_rows]
    all_pi_red_30s = [float(r["pi_red_30s"]) for r in all_rows]
    all_pi_ir_60s = [float(r["pi_ir_60s"]) for r in all_rows]

    def pi_stats(label, values):
        sorted_vals = sorted(values)
        n = len(sorted_vals)
        print(f"  PI {label}: min={sorted_vals[0]:.6f} p25={sorted_vals[n//4]:.6f} "
              f"mediana={sorted_vals[n//2]:.6f} p75={sorted_vals[3*n//4]:.6f} max={sorted_vals[-1]:.6f} "
              f"media={statistics.mean(values):.6f}")

    print("distribuicao do Indice de Perfusao (todas as janelas, todos os registros):")
    pi_stats("vermelho 30s", all_pi_red_30s)
    pi_stats("ir 30s", all_pi_ir_30s)
    pi_stats("ir 60s", all_pi_ir_60s)

    print()
    print("contagem de status de FC (hr_status), total:")
    for status, count in Counter(r["hr_status"] for r in all_rows).most_common():
        print(f"  {status}: {count} ({pct(count, total_windows):.2f}%)")

    print()
    print("contagem de status de FR fundida (rr_status), total:")
    for status, count in Counter(r["rr_status"] for r in all_rows).most_common():
        print(f"  {status}: {count} ({pct(count, total_windows):.2f}%)")

    print()
    print("contagem de status por modulacao de FR, total:")
    for col in ("rr_amplitude_status", "rr_frequency_status", "rr_baseline_status"):
        print(f"  {col}:")
        for status, count in Counter(r[col] for r in all_rows).most_common():
            print(f"    {status}: {count} ({pct(count, total_windows):.2f}%)")

    print()
    print("=== confirmacao de janela/hop (amostra: primeiro registro processado) ===")
    if per_record:
        sample_record = per_record[0]["record_id"]
        sample_params_path = out_dir / sample_record / f"{sample_record}_ppg_parameters.csv"
        sample_rows = read_params_csv(sample_params_path)
        print(f"registro {sample_record}, {len(sample_rows)} janelas:")
        for i, row in enumerate(sample_rows[:6]):
            start_s = float(row["start_time_s"])
            end_s = float(row["end_time_s"])
            width = end_s - start_s
            hop = None
            if i > 0:
                prev_end = float(sample_rows[i - 1]["end_time_s"])
                hop = end_s - prev_end
            print(f"  janela {i}: start={start_s}s end={end_s}s largura={width}s"
                  + (f" hop_desde_anterior={hop}s" if hop is not None else ""))

    return per_record, all_rows


def median_iqr(values):
    """Mediana e intervalo interquartil (p25,p75), ignorando NaN -- formato
    usado na literatura com a BIDMC (mediana por registro, não a média, por
    ser mais robusta a um registro atípico)."""
    arr = np.asarray(values, dtype=float)
    arr = arr[~np.isnan(arr)]
    if arr.size == 0:
        return float("nan"), float("nan"), float("nan"), 0
    return float(np.median(arr)), float(np.percentile(arr, 25)), float(np.percentile(arr, 75)), int(arr.size)


def rpm_distribution(rows, status_column, value_column, label):
    accepted = [float(r[value_column]) for r in rows if r[status_column] == "aceita" and r[value_column] != ""]
    if not accepted:
        print(f"  {label}: nenhuma janela aceita")
        return
    arr = np.array(accepted)
    n_6_8 = int(np.sum((arr >= 6.0) & (arr <= 8.0)))
    print(f"  {label}: n={len(accepted)}, media={arr.mean():.4f}, desvio={arr.std(ddof=1) if len(accepted) > 1 else float('nan'):.4f}, "
          f"min={arr.min():.4f}, max={arr.max():.4f}, entre_6_e_8_rpm={n_6_8} ({pct(n_6_8, len(accepted)):.1f}%)")


def phase2_validation(records, prepared_dir, out_dir, tcc_binary):
    prepared_dir = Path(prepared_dir)
    out_dir = Path(out_dir)
    aggregate_dir = out_dir / "aggregate"
    aggregate_dir.mkdir(parents=True, exist_ok=True)

    per_record_summaries = {name: [] for name in PARAMETERS}
    pooled_details = {name: [] for name in PARAMETERS}
    all_rows = []
    excluded_records = []  # (record_id, motivo) -- registro inteiro nao processado

    for record_id in records:
        ppg_csv = prepared_dir / f"{record_id}_ppg_prepared.csv"
        reference_csv = prepared_dir / f"{record_id}_reference_prepared.csv"
        record_out_dir = out_dir / record_id
        record_out_dir.mkdir(parents=True, exist_ok=True)
        params_path = record_out_dir / f"{record_id}_ppg_parameters.csv"

        result = run_pipeline(tcc_binary, ppg_csv, params_path)
        if result.returncode != 0:
            excluded_records.append((record_id, f"pipeline retornou codigo {result.returncode}"))
            print(f"ERRO ao rodar o pipeline para {record_id}:", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            continue

        all_rows.extend(read_params_csv(params_path))
        start_time = resolve_start_time(str(ppg_csv))

        for param_name, cfg in PARAMETERS.items():
            detail_df, summary_df, _, _ = validate_parameter(
                str(params_path), str(reference_csv),
                value_column=cfg["value_column"], status_column=cfg["status_column"], ref_column=cfg["ref_column"],
                extra_count_column=cfg["extra_count_column"], window_duration_s=cfg["window_duration_s"],
                start_time=start_time, out_dir=record_out_dir, out_prefix=f"{record_id}_{param_name}",
            )
            row = summary_df.iloc[0].to_dict()
            row["record_id"] = record_id
            per_record_summaries[param_name].append(row)

            detail_df = detail_df.copy()
            detail_df["record_id"] = record_id
            pooled_details[param_name].append(detail_df)

        print(f"{record_id}: processado ({len(PARAMETERS)} parametros validados)")

    print()
    if excluded_records:
        print("=== registros excluidos (pipeline nao executou) ===")
        for record_id, reason in excluded_records:
            print(f"  {record_id}: {reason}")
    else:
        print("nenhum registro excluido por falha de execucao do pipeline")

    param_exclusions = {name: [] for name in PARAMETERS}  # (record_id, motivo) -- so esse parametro

    for param_name in PARAMETERS:
        per_record_df = pd.DataFrame(per_record_summaries[param_name])
        per_record_path = aggregate_dir / f"{param_name}_per_record.csv"
        per_record_df.to_csv(per_record_path, index=False)

        print()
        print(f"=== {param_name}: tabela por registro (mae, bias, n_comparavel, cobertura) ===")
        table_cols = ["record_id", "mae", "bias", "n_comparavel", "n_descartada", "n_sem_calculo",
                      "n_sem_referencia", "coverage_pct"]
        print(per_record_df[table_cols].to_string(index=False))

        for _, row in per_record_df.iterrows():
            if row["n_comparavel"] == 0:
                param_exclusions[param_name].append((row["record_id"], "0 janelas comparaveis (sem referencia no intervalo ou nenhum valor calculado)"))

        median_mae, p25_mae, p75_mae, n_included = median_iqr(per_record_df["mae"])
        median_bias, p25_bias, p75_bias, _ = median_iqr(per_record_df["bias"])

        pooled_df = pd.concat(pooled_details[param_name], ignore_index=True)
        grouped_summary = summarize(pooled_df)
        grouped_summary.to_csv(aggregate_dir / f"{param_name}_grouped.csv", index=False)
        grouped = grouped_summary.iloc[0]

        print()
        print(f"=== {param_name}: agregado ({n_included}/{len(records) - len(excluded_records)} registros com janelas comparaveis) ===")
        print(f"  mediana do MAE por registro: {median_mae:.4f} (IQR {p25_mae:.4f}-{p75_mae:.4f})")
        print(f"  mediana do vies por registro: {median_bias:.4f} (IQR {p25_bias:.4f}-{p75_bias:.4f})")
        print(f"  agrupado (todas as janelas comparaveis, todos os registros): "
              f"mae={grouped['mae']:.4f} rmse={grouped['rmse']:.4f} bias={grouped['bias']:.4f} "
              f"pearson_r={grouped['pearson_r']:.4f} spearman_r={grouped['spearman_r']:.4f} "
              f"pct_erro_le_2={grouped['pct_error_le_2']:.2f}% pct_erro_le_3={grouped['pct_error_le_3']:.2f}% "
              f"cobertura={grouped['coverage_pct']:.2f}% (n_comparavel={int(grouped['n_comparavel'])} de {int(grouped['n_total'])})")

        if param_exclusions[param_name]:
            print(f"  registros excluidos desta mediana (0 janelas comparaveis): "
                  f"{', '.join(r for r, _ in param_exclusions[param_name])}")

    print()
    print("=== FR: distribuicao dos valores aceitos, pooled em todos os registros ===")
    rpm_distribution(all_rows, "rr_status", "rr_value", "fundida (rr_status==aceita)")
    rpm_distribution(all_rows, "rr_amplitude_status", "rr_amplitude_value", "amplitude")
    rpm_distribution(all_rows, "rr_frequency_status", "rr_frequency_value", "frequencia")
    rpm_distribution(all_rows, "rr_baseline_status", "rr_baseline_value", "linha de base")

    print()
    print("=== lista de registros excluidos e motivo (resumo) ===")
    if excluded_records:
        for record_id, reason in excluded_records:
            print(f"  {record_id}: excluido de TODOS os parametros -- {reason}")
    for param_name, exclusions in param_exclusions.items():
        for record_id, reason in exclusions:
            print(f"  {record_id}: excluido da mediana de {param_name} -- {reason}")
    if not excluded_records and not any(param_exclusions.values()):
        print("  nenhum")

    return per_record_summaries, pooled_details, excluded_records, param_exclusions


def reference_median_per_window(windows_df, reference_df, ref_column):
    """Mediana de referência por janela, sem depender do status/valor calculado
    pelo pipeline -- usa um status/valor "dummy" (sempre "aceita"/0.0) só pra
    forçar classify_windows a casar a referência em toda janela, baseado só no
    alinhamento temporal de 60s. Usada para comparar referências entre si."""
    dummy_df = windows_df.copy()
    dummy_df["dummy_status"] = "aceita"
    dummy_df["dummy_value"] = 0.0
    detail = classify_windows(
        dummy_df, reference_df,
        value_column="dummy_value", status_column="dummy_status", ref_column=ref_column,
    )
    return detail["value_reference_median"]


def compare_references(windows_df, ref_a_df, ref_b_df):
    """Diferença (a - b) por janela entre duas referências já alinhadas pela
    mesma janela de 60s; só nas janelas em que AMBAS tiveram mediana (ambas
    "comparavel", ver reference_median_per_window)."""
    med_a = reference_median_per_window(windows_df, ref_a_df, "rr_ref")
    med_b = reference_median_per_window(windows_df, ref_b_df, "rr_ref")
    both_present = med_a.notna() & med_b.notna()
    diff = (med_a - med_b)[both_present]
    return diff, int(both_present.sum()), len(windows_df)


def phase3_breath_validation(records, prepared_dir, fase2_out_dir, out_dir):
    prepared_dir = Path(prepared_dir)
    fase2_out_dir = Path(fase2_out_dir)
    out_dir = Path(out_dir)
    aggregate_dir = out_dir / "aggregate"
    aggregate_dir.mkdir(parents=True, exist_ok=True)

    fr_params = {name: cfg for name, cfg in PARAMETERS.items() if name != "hr"}

    per_record_summaries = {(name, ann): [] for name in fr_params for ann in BREATH_ANNOTATORS}
    pooled_details = {k: [] for k in per_record_summaries}

    concordance_rows = {"ann1_vs_ann2": [], "resp_vs_ann1": [], "resp_vs_ann2": []}

    skipped_records = []  # (record_id, motivo) -- Fase 2 nao foi rodada / referencia ausente

    for record_id in records:
        params_path = fase2_out_dir / record_id / f"{record_id}_ppg_parameters.csv"
        if not params_path.exists():
            skipped_records.append((record_id, "ppg_parameters.csv da Fase 2 nao encontrado -- rode --phase 2 primeiro"))
            continue

        ppg_csv = prepared_dir / f"{record_id}_ppg_prepared.csv"
        resp_reference_csv = prepared_dir / f"{record_id}_reference_prepared.csv"
        ann1_csv = prepared_dir / f"{record_id}_reference_breath_ann1.csv"
        ann2_csv = prepared_dir / f"{record_id}_reference_breath_ann2.csv"
        missing = [str(p) for p in (resp_reference_csv, ann1_csv, ann2_csv) if not p.exists()]
        if missing:
            skipped_records.append((record_id, f"referencia ausente: {', '.join(missing)}"))
            continue

        start_time = resolve_start_time(str(ppg_csv))
        record_out_dir = out_dir / record_id
        record_out_dir.mkdir(parents=True, exist_ok=True)

        for annotator in BREATH_ANNOTATORS:
            breath_ref_csv = prepared_dir / f"{record_id}_reference_breath_{annotator}.csv"
            for param_name, cfg in fr_params.items():
                detail_df, summary_df, _, _ = validate_parameter(
                    str(params_path), str(breath_ref_csv),
                    value_column=cfg["value_column"], status_column=cfg["status_column"], ref_column=cfg["ref_column"],
                    extra_count_column=cfg["extra_count_column"], window_duration_s=cfg["window_duration_s"],
                    start_time=start_time, out_dir=record_out_dir,
                    out_prefix=f"{record_id}_{param_name}_breath_{annotator}",
                )
                row = summary_df.iloc[0].to_dict()
                row["record_id"] = record_id
                per_record_summaries[(param_name, annotator)].append(row)
                detail_df = detail_df.copy()
                detail_df["record_id"] = record_id
                pooled_details[(param_name, annotator)].append(detail_df)

        windows_df = _load_windows(str(params_path), start_time, window_duration_s=RR_WINDOW_DURATION_S)
        resp_ref_df = _load_reference(str(resp_reference_csv), "rr_ref")
        ann1_ref_df = _load_reference(str(ann1_csv), "rr_ref")
        ann2_ref_df = _load_reference(str(ann2_csv), "rr_ref")

        for key, (ref_a, ref_b) in (
            ("ann1_vs_ann2", (ann1_ref_df, ann2_ref_df)),
            ("resp_vs_ann1", (resp_ref_df, ann1_ref_df)),
            ("resp_vs_ann2", (resp_ref_df, ann2_ref_df)),
        ):
            diff, n_compared, n_total_windows = compare_references(windows_df, ref_a, ref_b)
            if n_compared == 0:
                concordance_rows[key].append({
                    "record_id": record_id, "median_abs_diff": float("nan"), "bias": float("nan"),
                    "pct_le_2": float("nan"), "n_compared": 0, "n_total_windows": n_total_windows,
                })
                continue
            abs_diff = diff.abs()
            concordance_rows[key].append({
                "record_id": record_id,
                "median_abs_diff": float(abs_diff.median()),
                "bias": float(diff.mean()),
                "pct_le_2": float(pct(int((abs_diff <= 2.0).sum()), n_compared)),
                "n_compared": n_compared,
                "n_total_windows": n_total_windows,
            })

        print(f"{record_id}: validado contra ann1 e ann2; concordancia entre referencias calculada")

    print()
    if skipped_records:
        print("=== registros pulados (sem dados para a Fase 3) ===")
        for record_id, reason in skipped_records:
            print(f"  {record_id}: {reason}")
    else:
        print("nenhum registro pulado")

    print()
    print("=== FR contra anotacoes manuais: agregado por parametro/anotador ===")
    param_exclusions = {k: [] for k in per_record_summaries}
    for (param_name, annotator), rows in per_record_summaries.items():
        per_record_df = pd.DataFrame(rows)
        per_record_df.to_csv(aggregate_dir / f"{param_name}_breath_{annotator}_per_record.csv", index=False)

        for _, row in per_record_df.iterrows():
            if row["n_comparavel"] == 0:
                param_exclusions[(param_name, annotator)].append(
                    (row["record_id"], "0 janelas comparaveis contra essa referencia"))

        median_mae, p25_mae, p75_mae, n_included = median_iqr(per_record_df["mae"])
        pooled_df = pd.concat(pooled_details[(param_name, annotator)], ignore_index=True)
        grouped = summarize(pooled_df)
        grouped.to_csv(aggregate_dir / f"{param_name}_breath_{annotator}_grouped.csv", index=False)
        g = grouped.iloc[0]

        print(f"  {param_name} vs {annotator}: mediana_MAE={median_mae:.4f} (IQR {p25_mae:.4f}-{p75_mae:.4f}, "
              f"n={n_included} registros) | agrupado: mae={g['mae']:.4f} rmse={g['rmse']:.4f} bias={g['bias']:.4f} "
              f"pearson_r={g['pearson_r']:.4f} spearman_r={g['spearman_r']:.4f} cobertura={g['coverage_pct']:.2f}% "
              f"(n_comparavel={int(g['n_comparavel'])} de {int(g['n_total'])})")

    print()
    print("=== item 4: concordancia entre referencias (mesmo alinhamento de 60s) ===")
    for key, rows in concordance_rows.items():
        df = pd.DataFrame(rows)
        df.to_csv(aggregate_dir / f"concordance_{key}.csv", index=False)
        median_d, p25_d, p75_d, n_included = median_iqr(df["median_abs_diff"])
        valid = df[df["n_compared"] > 0]
        pooled_bias = float(np.average(valid["bias"], weights=valid["n_compared"])) if len(valid) else float("nan")
        pooled_pct_le_2 = float(np.average(valid["pct_le_2"], weights=valid["n_compared"])) if len(valid) else float("nan")
        print(f"  {key}: mediana da diferenca absoluta por registro = {median_d:.4f} rpm (IQR {p25_d:.4f}-{p75_d:.4f}, "
              f"n={n_included} registros) | vies medio (ponderado por janelas) = {pooled_bias:.4f} rpm | "
              f"%janelas com diferenca<=2rpm (ponderado) = {pooled_pct_le_2:.2f}%")
        zero_compared = df[df["n_compared"] == 0]
        if len(zero_compared):
            print(f"    registros sem nenhuma janela comparavel nesta comparacao: "
                  f"{', '.join(zero_compared['record_id'])}")

    print()
    print("=== registros/janelas excluidos (resumo) ===")
    if skipped_records:
        for record_id, reason in skipped_records:
            print(f"  {record_id}: {reason}")
    for (param_name, annotator), exclusions in param_exclusions.items():
        for record_id, reason in exclusions:
            print(f"  {record_id}: excluido da mediana de {param_name} vs {annotator} -- {reason}")
    if not skipped_records and not any(param_exclusions.values()):
        print("  nenhum, alem do relatado na secao de concordancia acima")

    return per_record_summaries, pooled_details, concordance_rows, skipped_records, param_exclusions


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("records", nargs="*", help="registros especificos (ex.: bidmc01); default: todos em --prepared-dir")
    parser.add_argument("--prepared-dir", default="../../data/bidmc/prepared")
    parser.add_argument("--out-dir", default="../../debug_output/bidmc")
    parser.add_argument("--tcc-binary", default="../../build/tcc")
    parser.add_argument("--phase", choices=["1", "2", "3"], default="1",
                         help="1 (default): so diagnostico, sem comparar com referencia. "
                              "2: validacao completa contra a referencia (RESP/HR do monitor) -- so rodar apos "
                              "confirmacao. 3: validacao da FR contra as anotacoes manuais de respiracao, "
                              "reaproveitando os ppg_parameters.csv da Fase 2 (nao roda o pipeline de novo).")
    parser.add_argument("--breath-out-dir", default="../../debug_output/bidmc/aggregate_breath_annotations",
                         help="saida da Fase 3 (default: debug_output/bidmc/aggregate_breath_annotations)")
    args = parser.parse_args()

    records = args.records or find_prepared_records(args.prepared_dir)
    if not records:
        print(f"nenhum registro preparado encontrado em {args.prepared_dir}", file=sys.stderr)
        return 1

    print(f"{len(records)} registro(s)\n")

    if args.phase == "1":
        phase1_report(records, args.prepared_dir, args.out_dir, args.tcc_binary)
        return 0

    if args.phase == "2":
        phase2_validation(records, args.prepared_dir, args.out_dir, args.tcc_binary)
        return 0

    phase3_breath_validation(records, args.prepared_dir, args.out_dir, args.breath_out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
