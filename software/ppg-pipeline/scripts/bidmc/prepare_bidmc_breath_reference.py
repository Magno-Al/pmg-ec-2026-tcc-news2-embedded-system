"""Gera, para cada registro da BIDMC e cada um dos dois anotadores, uma
referência de FR derivada das anotações MANUAIS de respiração (ver
data/bidmc/raw/bidmc##.breath) -- uma referência complementar, mais rigorosa
que a coluna RESP do monitor (que já é validada separadamente, ver
prepare_bidmc_data.py).

## Formato das anotações

Fonte principal: WFDB (bidmc##.breath), lido com wfdb.rdann(record, "breath").
Cada marca tem uma posição (amostra, na taxa do sinal) e o campo aux_note vale
"ann1" ou "ann2" -- os dois fluxos vêm intercalados, na ordem temporal, sem
separação por posição no arquivo. Há também um CSV equivalente
(bidmc_csv/bidmc_##_Breaths.csv, 1-based, um anotador por coluna), não usado
aqui: conferência em 3 registros mostrou que o CSV tem algumas marcações
extras de ann1 que não existem no WFDB (divergência pequena entre os formatos).

## Referência gerada

Para cada registro e cada anotador, a partir da SEGUNDA respiração anotada:
    rr_instantanea[i] = 60 / (t[i] - t[i-1])   (rpm; t em segundos)
posicionada no instante t[i] da própria respiração. t[i] vem da posição em
amostras (lida do WFDB) dividida pela taxa REAL do registro, lida do cabeçalho
bidmc##.hea (nunca um valor fixo) -- ver EXPECTED_SIGNAL_RATE_HZ, usado só
para a mesma checagem de consistência já feita em prepare_bidmc_data.py.

Gera, por registro e por anotador:

  <out_dir>/bidmc##_reference_breath_ann1.csv   timestamp_abs, rr_ref
  <out_dir>/bidmc##_reference_breath_ann2.csv   timestamp_abs, rr_ref

timestamp_abs usa a MESMA data-base fixa de prepare_bidmc_data.py
(TIMESTAMP_BASE, importada daquele módulo -- não redeclarada aqui, para as
três referências de um mesmo registro ficarem no mesmo relógio fictício e
permanecerem comparáveis).

IMPORTANTE: como o núcleo de validação (scripts/common/ppg_validation.py,
inalterado) agrega os pontos de referência que caem dentro de uma janela pela
MEDIANA, a "FR da janela" vinda desta referência passa a ser a mediana das FR
instantâneas das respirações anotadas dentro daquela janela de 60s -- não é
um cálculo novo feito aqui, é simplesmente o que classify_windows já faz com
qualquer referência de mais de uma amostra por janela.

Ferramenta offline de preparação de dados (Python puro) — não faz parte do
pipeline C++ embarcado, só alimenta a etapa de validação.

Uso (rode a partir deste diretório, scripts/bidmc, ou ajuste --raw-dir/--out-dir):
    python prepare_bidmc_breath_reference.py [registro ...] [--raw-dir DIR] [--out-dir DIR]
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import wfdb

from prepare_bidmc_data import TIMESTAMP_BASE, TIMESTAMP_FORMAT, check_rate, find_records

EXPECTED_SIGNAL_RATE_HZ = 125
ANNOTATORS = ("ann1", "ann2")


def instantaneous_rr(breath_times_s):
    """A partir da segunda respiração: 60 / intervalo até a anterior (rpm),
    posicionado no instante da respiração atual. breath_times_s já ordenado."""
    times = np.asarray(sorted(breath_times_s))
    if len(times) < 2:
        return np.array([]), np.array([])
    intervals_s = np.diff(times)
    rr_rpm = 60.0 / intervals_s
    return times[1:], rr_rpm


def prepare_record_breath_reference(record_id, raw_dir, out_dir):
    raw_dir = Path(raw_dir)
    signals = wfdb.rdrecord(str(raw_dir / record_id))
    check_rate(signals, EXPECTED_SIGNAL_RATE_HZ, record_id, "sinais", "")
    sample_rate_hz = signals.fs

    ann = wfdb.rdann(str(raw_dir / record_id), "breath")

    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    info = {"record_id": record_id, "sample_rate_hz": sample_rate_hz}
    for annotator in ANNOTATORS:
        samples = [ann.sample[i] for i in range(len(ann.sample)) if ann.aux_note[i] == annotator]
        if len(samples) < 2:
            raise ValueError(f"{record_id}/{annotator}: menos de 2 respiracoes anotadas, nao e possivel "
                              f"calcular nenhuma FR instantanea")

        breath_times_s = np.asarray(sorted(samples)) / sample_rate_hz
        rr_times_s, rr_rpm = instantaneous_rr(breath_times_s)

        offsets = pd.to_timedelta(rr_times_s, unit="s")
        reference_df = pd.DataFrame({
            "timestamp_abs": TIMESTAMP_BASE + offsets,
            "rr_ref": rr_rpm,
        })

        out_path = out_dir / f"{record_id}_reference_breath_{annotator}.csv"
        reference_df.to_csv(out_path, index=False, date_format=TIMESTAMP_FORMAT)

        info[f"{annotator}_n_breaths"] = len(samples)
        info[f"{annotator}_n_instantaneous"] = len(rr_rpm)
        info[f"{annotator}_path"] = out_path

    return info


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
            info = prepare_record_breath_reference(record_id, args.raw_dir, args.out_dir)
        except ValueError as exc:
            print(f"ERRO: {exc}", file=sys.stderr)
            return 1

        print(f"{record_id}: taxa={info['sample_rate_hz']}Hz, "
              f"ann1={info['ann1_n_breaths']} respiracoes ({info['ann1_n_instantaneous']} FR instantaneas), "
              f"ann2={info['ann2_n_breaths']} respiracoes ({info['ann2_n_instantaneous']} FR instantaneas)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
