"""Prepara os dados brutos de um encounter do OpenOximetry para validação.

Gera, a partir de <id>_ppg.dat/.hea (sinal PPG bruto, formato WFDB) e
<id>_2hz.csv (referências de vários dispositivos, 2 Hz), três arquivos:

  <id>_ppg_prepared.csv        timestamp_abs, red_signal, ir_signal
  <id>_ppg_prepared.meta       sample_rate_hz=<taxa real, lida do .hea>
                                raw_intensity=true
  <id>_reference_prepared.csv  timestamp_abs, spo2_ref, hr_ref, rr_ref

O .meta é o formato comum de entrada do pipeline C++ (ver README, seção 4):
sample_rate_hz vem de record.fs (wfdb), não hardcoded. raw_intensity=true
porque o canal IR do OpenOximetry é a intensidade bruta do fotodetector.

Ferramenta offline de preparação de dados (Python puro) — não faz parte do
pipeline C++ embarcado, só alimenta a etapa de validação (fora de escopo aqui).

Uso (rode a partir deste diretório, scripts/openoximetry, ou ajuste --data-dir/
--out-dir):
    python prepare_openoximetry_data.py <encounter_id_ou_caminho> [--data-dir DIR] [--out-dir DIR]
"""

import argparse
from pathlib import Path

import pandas as pd
import wfdb

RED_SIGNAL_NAME = "Red Signal"
IR_SIGNAL_NAME = "IR Signal"
TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S.%f"


def resolve_encounter(encounter, data_dir):
    """Aceita um id de encounter (procurado em data_dir) ou um caminho/prefixo
    direto, com ou sem o sufixo _ppg / _2hz.csv."""
    path = Path(encounter)
    name = path.name
    for suffix in ("_ppg", "_2hz.csv"):
        if name.endswith(suffix):
            name = name[: -len(suffix)]

    # Sem componente de diretório no argumento (id "puro"): procura em data_dir.
    directory = path.parent if str(path.parent) != "." else Path(data_dir)
    base = directory / name

    ppg_record = base.with_name(base.name + "_ppg")
    reference_csv = base.with_name(base.name + "_2hz.csv")

    if not ppg_record.with_suffix(".hea").exists():
        raise FileNotFoundError(f"header do PPG não encontrado: {ppg_record.with_suffix('.hea')}")
    if not reference_csv.exists():
        raise FileNotFoundError(f"CSV de referência não encontrado: {reference_csv}")

    return base.name, ppg_record, reference_csv


def load_ppg(ppg_record_path, reference_start_timestamp):
    record = wfdb.rdrecord(str(ppg_record_path))

    red_index = record.sig_name.index(RED_SIGNAL_NAME)
    ir_index = record.sig_name.index(IR_SIGNAL_NAME)

    # O .hea só tem a hora (sem data); a data vem da primeira amostra do _2hz. Se
    # compor a data da referência com a hora do PPG resultar num instante
    # posterior à própria referência, a meia-noite ficou entre os dois: o PPG
    # (que começa um pouco antes, por warm-up do sensor) começou no dia anterior.
    start_datetime = pd.Timestamp.combine(reference_start_timestamp.date(), record.base_time)
    if start_datetime > reference_start_timestamp:
        start_datetime -= pd.Timedelta(days=1)

    offsets = pd.to_timedelta(pd.RangeIndex(record.sig_len) / record.fs, unit="s")
    timestamps = start_datetime + offsets

    ppg_df = pd.DataFrame({
        "timestamp_abs": timestamps,
        "red_signal": record.p_signal[:, red_index],
        "ir_signal": record.p_signal[:, ir_index],
    })
    return ppg_df, record.fs


def load_reference(reference_csv_path):
    df = pd.read_csv(reference_csv_path)

    timestamps = pd.to_datetime(df["Timestamp"])

    spo2_columns = [c for c in df.columns if "SpO2" in c]
    hr_columns = [c for c in df.columns if "_HR" in c]

    spo2_ref = df[spo2_columns].apply(pd.to_numeric, errors="coerce").mean(axis=1) if spo2_columns else pd.Series(float("nan"), index=df.index)
    hr_ref = df[hr_columns].apply(pd.to_numeric, errors="coerce").mean(axis=1) if hr_columns else pd.Series(float("nan"), index=df.index)

    if "RR" in df.columns:
        rr_ref = pd.to_numeric(df["RR"], errors="coerce")
    else:
        print("aviso: coluna 'RR' não encontrada neste encounter, rr_ref ficará vazio")
        rr_ref = pd.Series(float("nan"), index=df.index)

    print(f"colunas de SpO2 usadas: {spo2_columns or '(nenhuma encontrada)'}")
    print(f"colunas de HR usadas: {hr_columns or '(nenhuma encontrada)'}")

    return pd.DataFrame({
        "timestamp_abs": timestamps,
        "spo2_ref": spo2_ref,
        "hr_ref": hr_ref,
        "rr_ref": rr_ref,
    })


def print_summary(encounter_id, ppg_df, reference_df):
    ppg_duration_s = (ppg_df["timestamp_abs"].iloc[-1] - ppg_df["timestamp_abs"].iloc[0]).total_seconds()

    print(f"\n=== resumo: {encounter_id} ===")
    print(f"PPG: {len(ppg_df)} amostras, {ppg_duration_s:.1f}s "
          f"({ppg_df['timestamp_abs'].iloc[0]} até {ppg_df['timestamp_abs'].iloc[-1]})")
    print(f"referência: {len(reference_df)} amostras "
          f"({reference_df['timestamp_abs'].iloc[0]} até {reference_df['timestamp_abs'].iloc[-1]})")
    for column in ("spo2_ref", "hr_ref", "rr_ref"):
        filled = reference_df[column].notna().sum()
        print(f"  {column}: {filled} de {len(reference_df)} linhas preenchidas "
              f"({100 * filled / len(reference_df):.1f}%)")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("encounter", help="id do encounter (ex.: 01a542fe...) ou caminho/prefixo dos arquivos")
    parser.add_argument("--data-dir", default="../../data/openoximetry/raw",
                         help="diretório onde procurar o encounter por id (default: ../../data/openoximetry/raw)")
    parser.add_argument("--out-dir", default="../../data/openoximetry/prepared",
                         help="diretório de saída dos arquivos preparados (default: ../../data/openoximetry/prepared)")
    args = parser.parse_args()

    encounter_id, ppg_record_path, reference_csv_path = resolve_encounter(args.encounter, args.data_dir)

    reference_df = load_reference(reference_csv_path)
    ppg_df, sample_rate_hz = load_ppg(ppg_record_path, reference_df["timestamp_abs"].iloc[0])

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    ppg_out_path = out_dir / f"{encounter_id}_ppg_prepared.csv"
    meta_out_path = out_dir / f"{encounter_id}_ppg_prepared.meta"
    reference_out_path = out_dir / f"{encounter_id}_reference_prepared.csv"

    ppg_df.to_csv(ppg_out_path, index=False, date_format=TIMESTAMP_FORMAT)
    meta_out_path.write_text(f"sample_rate_hz={sample_rate_hz}\nraw_intensity=true\n")
    reference_df.to_csv(reference_out_path, index=False, date_format=TIMESTAMP_FORMAT)

    print(f"salvo em {ppg_out_path}")
    print(f"salvo em {meta_out_path} (sample_rate_hz={sample_rate_hz}, raw_intensity=true)")
    print(f"salvo em {reference_out_path}")

    print_summary(encounter_id, ppg_df, reference_df)


if __name__ == "__main__":
    main()
