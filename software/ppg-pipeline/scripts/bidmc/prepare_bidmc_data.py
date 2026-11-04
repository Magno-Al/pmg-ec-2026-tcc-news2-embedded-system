"""Prepara os dados brutos da base BIDMC PPG and Respiration Dataset para o
pipeline C++ (ver README, seção 4, "Formato de entrada").

Lê os arquivos WFDB de cada registro:

  <raw_dir>/bidmc##.dat/.hea    sinais a 125 Hz, incluindo o canal PPG (PLETH)
  <raw_dir>/bidmc##n.dat/.hea   parâmetros a 1 Hz, incluindo HR e RESP

E gera, por registro:

  <out_dir>/bidmc##_ppg_prepared.csv    timestamp_abs, ir_signal (SEM red_signal
                                         -- desativa o SpO2, ver PPGSourceInfo)
  <out_dir>/bidmc##_ppg_prepared.meta   sample_rate_hz=<lido do .hea de bidmc##>
                                         raw_intensity=false
  <out_dir>/bidmc##_reference_prepared.csv   timestamp_abs, hr_ref, rr_ref

raw_intensity=false: o PLETH da BIDMC é documentado em NU (unidades
normalizadas pelo monitor de origem) no cabeçalho WFDB -- confirmado por
registro antes de gravar (ver EXPECTED_PPG_UNIT); ver README, seção 4.

hr_ref vem da coluna HR (derivada do ECG pelo monitor); rr_ref vem da coluna
RESP dos PARÂMETROS (bidmc##n, FR derivada pela impedância torácica) -- não a
coluna RESP dos SINAIS (bidmc##, forma de onda, não frequência) nem a coluna
PULSE (derivada do próprio PPG). As anotações manuais (bidmc##.breath) não são
usadas aqui.

Ferramenta offline de preparação de dados (Python puro) — não faz parte do
pipeline C++ embarcado, só alimenta a etapa de validação.

Uso (rode a partir deste diretório, scripts/bidmc, ou ajuste --raw-dir/--out-dir):
    python prepare_bidmc_data.py [registro ...] [--raw-dir DIR] [--out-dir DIR]
    (sem argumentos de registro: prepara todos os encontrados em --raw-dir)
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import wfdb

PPG_CHANNEL_NAME = "PLETH"
HR_CHANNEL_NAME = "HR"
RESP_CHANNEL_NAME = "RESP"

EXPECTED_SIGNAL_RATE_HZ = 125
EXPECTED_NUMERICS_RATE_HZ = 1

# Unidade em que o PLETH precisa estar documentado no .hea para que
# raw_intensity seja gravado como falso -- declaração lida do cabeçalho WFDB,
# nunca inferida dos valores do sinal.
EXPECTED_PPG_UNIT = "NU"

TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S.%f"

# A BIDMC só tem tempo relativo (sem data/hora absoluta no .hea); data-base
# fixa e arbitrária, igual para todos os registros, só para satisfazer o
# formato timestamp_abs do núcleo de validação.
TIMESTAMP_BASE = pd.Timestamp("2000-01-01")

# Fator multiplicativo (sem offset) aplicado ao PLETH (NU, ~0-4) antes de
# escrever em ir_signal (uint32_t): preserva exatamente a razão AC/DC do PI,
# já que PI = (k*AC)/(k*DC) = AC/DC para qualquer k>0.
PPG_SCALE_FACTOR = 100000


def clean_names(sig_names):
    return [name.strip().rstrip(",").strip() for name in sig_names]


def find_records(raw_dir):
    raw_dir = Path(raw_dir)
    records = []
    for path in sorted(raw_dir.glob("bidmc*.hea")):
        stem = path.stem
        suffix = stem[len("bidmc"):]
        if suffix.isdigit():  # exclui bidmc##n.hea (parâmetros)
            records.append(stem)
    return records


def check_rate(record_header, expected_hz, record_id, kind, header_suffix):
    if record_header.fs != expected_hz:
        raise ValueError(
            f"{record_id}: taxa de {kind} e {record_header.fs} Hz, esperado {expected_hz} Hz "
            f"(cabecalho {record_id}{header_suffix}.hea) -- preparacao interrompida, "
            f"a taxa real sempre vem do .hea, nunca de um valor fixo no script"
        )


def prepare_record(record_id, raw_dir, out_dir):
    raw_dir = Path(raw_dir)

    signals = wfdb.rdrecord(str(raw_dir / record_id))
    check_rate(signals, EXPECTED_SIGNAL_RATE_HZ, record_id, "sinais", "")
    sig_names = clean_names(signals.sig_name)
    if PPG_CHANNEL_NAME not in sig_names:
        raise ValueError(f"{record_id}: canal {PPG_CHANNEL_NAME} nao encontrado em {sig_names}")
    ppg_idx = sig_names.index(PPG_CHANNEL_NAME)
    ppg_unit = signals.units[ppg_idx].strip()
    if ppg_unit != EXPECTED_PPG_UNIT:
        raise ValueError(
            f"{record_id}: unidade do canal {PPG_CHANNEL_NAME} e {ppg_unit!r}, esperado {EXPECTED_PPG_UNIT!r} "
            f"(cabecalho {record_id}.hea) -- preparacao interrompida, raw_intensity so e gravado como falso "
            f"quando a unidade documentada no .hea for {EXPECTED_PPG_UNIT!r}"
        )
    ppg_physical = signals.p_signal[:, ppg_idx]
    if np.nanmin(ppg_physical) < 0:
        raise ValueError(f"{record_id}: PLETH tem valor negativo -- escala assume sinal nao-negativo")
    ppg_scaled = np.round(np.nan_to_num(ppg_physical, nan=0.0) * PPG_SCALE_FACTOR).astype(np.int64)

    signal_offsets = pd.to_timedelta(np.arange(signals.sig_len) / signals.fs, unit="s")
    ppg_df = pd.DataFrame({
        "timestamp_abs": TIMESTAMP_BASE + signal_offsets,
        "ir_signal": ppg_scaled,
    })

    numerics = wfdb.rdrecord(str(raw_dir / f"{record_id}n"))
    check_rate(numerics, EXPECTED_NUMERICS_RATE_HZ, record_id, "parametros", "n")
    num_names = clean_names(numerics.sig_name)
    for name in (HR_CHANNEL_NAME, RESP_CHANNEL_NAME):
        if name not in num_names:
            raise ValueError(f"{record_id}: canal {name} nao encontrado nos parametros {num_names}")
    hr_ref = numerics.p_signal[:, num_names.index(HR_CHANNEL_NAME)]
    rr_ref = numerics.p_signal[:, num_names.index(RESP_CHANNEL_NAME)]

    numerics_offsets = pd.to_timedelta(np.arange(numerics.sig_len) / numerics.fs, unit="s")
    reference_df = pd.DataFrame({
        "timestamp_abs": TIMESTAMP_BASE + numerics_offsets,
        "hr_ref": hr_ref,
        "rr_ref": rr_ref,
    })

    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    ppg_path = out_dir / f"{record_id}_ppg_prepared.csv"
    meta_path = out_dir / f"{record_id}_ppg_prepared.meta"
    reference_path = out_dir / f"{record_id}_reference_prepared.csv"

    ppg_df.to_csv(ppg_path, index=False, date_format=TIMESTAMP_FORMAT)
    meta_path.write_text(f"sample_rate_hz={signals.fs}\nraw_intensity=false\n")
    reference_df.to_csv(reference_path, index=False, date_format=TIMESTAMP_FORMAT)

    return {
        "record_id": record_id,
        "ppg_unit": ppg_unit,
        "ppg_path": ppg_path,
        "meta_path": meta_path,
        "reference_path": reference_path,
        "n_signal_samples": signals.sig_len,
        "n_numerics_samples": numerics.sig_len,
        "sample_rate_hz": signals.fs,
        "ppg_scaled_min": int(ppg_scaled.min()),
        "ppg_scaled_max": int(ppg_scaled.max()),
        "ppg_scaled_mean": float(ppg_scaled.mean()),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("records", nargs="*",
                         help="registros especificos (ex.: bidmc01 bidmc02); default: todos em --raw-dir")
    parser.add_argument("--raw-dir", default="../../data/bidmc/raw",
                         help="diretorio com os arquivos WFDB brutos (default: ../../data/bidmc/raw)")
    parser.add_argument("--out-dir", default="../../data/bidmc/prepared",
                         help="diretorio de saida (default: ../../data/bidmc/prepared)")
    args = parser.parse_args()

    records = args.records or find_records(args.raw_dir)
    if not records:
        print(f"nenhum registro encontrado em {args.raw_dir}", file=sys.stderr)
        return 1

    print(f"{len(records)} registro(s) a preparar")

    for record_id in records:
        try:
            info = prepare_record(record_id, args.raw_dir, args.out_dir)
        except ValueError as exc:
            print(f"ERRO: {exc}", file=sys.stderr)
            return 1

        print(f"{record_id}: {info['n_signal_samples']} amostras de sinal a {info['sample_rate_hz']} Hz, "
              f"unidade PLETH={info['ppg_unit']}, "
              f"{info['n_numerics_samples']} amostras de parametros, "
              f"ppg escalado min={info['ppg_scaled_min']} max={info['ppg_scaled_max']} "
              f"media={info['ppg_scaled_mean']:.1f} -> {info['ppg_path'].name}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
