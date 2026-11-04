"""Gera uma cópia dos registros preparados reamostrada para a taxa do equipamento.

O MAX30102 entrega 25 amostras/s ao app (100 Hz com média de 4 no próprio chip). Para
comparar os métodos nessa condição, cada bloco de N amostras da base vira uma, pela média,
como faz o sensor. A taxa de origem precisa ser múltipla da de destino.

Uso:
    python prepare_device_rate.py <prepared_dir> <saida_dir> [--rate 25]

Copia também as referências (*_reference*.csv), que não dependem da taxa do PPG.
"""

import argparse
import shutil
from pathlib import Path

import pandas as pd


def read_meta(path):
    meta = {}
    for line in Path(path).read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            meta[key] = value
    return meta


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("prepared_dir")
    parser.add_argument("out_dir")
    parser.add_argument("--rate", type=int, default=25)
    args = parser.parse_args()

    prepared_dir = Path(args.prepared_dir)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    for ppg_csv in sorted(prepared_dir.glob("*_ppg_prepared.csv")):
        meta = read_meta(ppg_csv.with_suffix(".meta"))
        source_rate = int(meta["sample_rate_hz"])
        if source_rate % args.rate != 0:
            raise SystemExit(f"{ppg_csv.name}: {source_rate} Hz não é múltiplo de {args.rate} Hz")
        block = source_rate // args.rate

        frame = pd.read_csv(ppg_csv)
        usable = len(frame) - len(frame) % block
        frame = frame.iloc[:usable]
        groups = frame.index // block
        signal_columns = [c for c in ("red_signal", "ir_signal") if c in frame.columns]
        reduced = frame.groupby(groups)[signal_columns].mean().round().astype("int64")
        reduced.insert(0, "timestamp_abs", frame.groupby(groups)["timestamp_abs"].first())
        reduced.to_csv(out_dir / ppg_csv.name, index=False)

        meta["sample_rate_hz"] = str(args.rate)
        (out_dir / ppg_csv.with_suffix(".meta").name).write_text(
            "".join(f"{key}={value}\n" for key, value in meta.items())
        )
        for reference in prepared_dir.glob(ppg_csv.name.replace("_ppg_prepared.csv", "_reference*.csv")):
            shutil.copy(reference, out_dir / reference.name)
        print(f"{ppg_csv.name}: {source_rate} Hz -> {args.rate} Hz ({len(reduced)} amostras)")


if __name__ == "__main__":
    main()
