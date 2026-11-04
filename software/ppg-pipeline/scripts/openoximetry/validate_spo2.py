"""Valida o SpO2 calculado pelo pipeline C++ contra a referência do OpenOximetry.

Chamada fina de ppg_validation.validate_parameter() com os nomes específicos de
SpO2 — a lógica de alinhamento/métricas/categorias é compartilhada com
validate_heart_rate.py.

Uso:
    python validate_spo2.py <ppg_parameters.csv> <reference.csv> --start-time <data/caminho> [--out-dir DIR]

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


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("params_csv", help="CSV de parâmetros por janela exportado pelo pipeline (PPG_PARAM_EXPORT=1)")
    parser.add_argument("reference_csv", help="CSV de referência (prepare_openoximetry_data.py)")
    parser.add_argument("--start-time", required=True,
                         help="data/hora de início do registro, ou caminho de um CSV com timestamp_abs "
                              "(ex.: o _ppg_prepared.csv do mesmo encounter)")
    parser.add_argument("--out-dir", default=None,
                         help="diretório de saída (default: debug_output/openoximetry/<encounter>)")
    args = parser.parse_args()

    start_time = resolve_start_time(args.start_time)
    print(f"horário de início do registro: {start_time}")

    out_dir = args.out_dir or Path("debug_output") / "openoximetry" / encounter_id_from_reference(args.reference_csv)
    out_prefix = derive_prefix(args.reference_csv, "spo2")

    _, summary_df, detail_path, summary_path = validate_parameter(
        args.params_csv, args.reference_csv,
        value_column="spo2_value", status_column="spo2_status", ref_column="spo2_ref",
        start_time=start_time, out_dir=out_dir, out_prefix=out_prefix,
    )

    print(f"salvo em {detail_path}")
    print(f"salvo em {summary_path}")
    print()
    print(summary_df.to_string(index=False))


if __name__ == "__main__":
    main()
