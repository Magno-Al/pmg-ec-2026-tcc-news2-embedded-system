"""Valida a FR calculada pelo pipeline C++ contra a referência do OpenOximetry.

--modulation escolhe qual das quatro colunas exportadas validar: o valor fundido
pelo Smart Fusion ("fused", default) ou uma das três modulações individuais.

A janela real da FR é de 60s (PPG_WINDOW_SECONDS_MAX), não os 30s que
start_time_s/end_time_s refletem no CSV -- por isso passamos
window_duration_s=RR_WINDOW_DURATION_S pra validate_parameter recalcular o início
real da janela antes de alinhar com a referência.

Uso:
    python validate_respiratory_rate.py <ppg_parameters.csv> <reference.csv> --start-time <data/caminho> [--modulation amplitude|frequency|baseline] [--out-dir DIR]

(rode a partir da raiz do projeto, ou os caminhos relativos default de --out-dir
não vão bater -- ver debug_output/openoximetry/<encounter>)
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "common"))
from ppg_validation import derive_prefix, resolve_start_time, validate_parameter  # noqa: E402


def encounter_id_from_reference(reference_csv_path):
    """<id>_reference_prepared.csv -> <id> (mesma convenção de derive_prefix)."""
    stem = Path(reference_csv_path).stem
    suffix = "_reference_prepared"
    return stem[: -len(suffix)] if stem.endswith(suffix) else stem

# Precisa bater com PPG_WINDOW_SECONDS_MAX em include/PPG.hpp.
RR_WINDOW_DURATION_S = 60.0

MODULATION_COLUMNS = {
    "fused": ("rr_value", "rr_status"),
    "amplitude": ("rr_amplitude_value", "rr_amplitude_status"),
    "frequency": ("rr_frequency_value", "rr_frequency_status"),
    "baseline": ("rr_baseline_value", "rr_baseline_status"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("params_csv", help="CSV de parâmetros por janela exportado pelo pipeline (PPG_PARAM_EXPORT=1)")
    parser.add_argument("reference_csv", help="CSV de referência (prepare_openoximetry_data.py)")
    parser.add_argument("--modulation", choices=sorted(MODULATION_COLUMNS), default="fused",
                         help="qual estimativa de FR validar: o valor fundido (fused, default) ou uma "
                              "das três modulações individuais (amplitude/frequency/baseline)")
    parser.add_argument("--start-time", required=True,
                         help="data/hora de início do registro, ou caminho de um CSV com timestamp_abs "
                              "(ex.: o _ppg_prepared.csv do mesmo encounter)")
    parser.add_argument("--out-dir", default=None,
                         help="diretório de saída (default: debug_output/openoximetry/<encounter>)")
    args = parser.parse_args()

    value_column, status_column = MODULATION_COLUMNS[args.modulation]

    start_time = resolve_start_time(args.start_time)
    print(f"horário de início do registro: {start_time}")
    print(f"modulação: {args.modulation} ({value_column}/{status_column})")

    out_dir = args.out_dir or Path("debug_output") / "openoximetry" / encounter_id_from_reference(args.reference_csv)
    out_prefix = derive_prefix(args.reference_csv, f"rr_{args.modulation}")

    _, summary_df, detail_path, summary_path = validate_parameter(
        args.params_csv, args.reference_csv,
        value_column=value_column, status_column=status_column, ref_column="rr_ref",
        extra_count_column=None,
        window_duration_s=RR_WINDOW_DURATION_S,
        start_time=start_time, out_dir=out_dir, out_prefix=out_prefix,
    )

    print(f"salvo em {detail_path}")
    print(f"salvo em {summary_path}")
    print()
    print(summary_df.to_string(index=False))


if __name__ == "__main__":
    main()
