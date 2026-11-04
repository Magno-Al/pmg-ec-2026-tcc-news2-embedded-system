#!/bin/sh
# Compara, na BIDMC, o pipeline deste diretório com o PpgProcessor do app monitor-news2.
# Três configurações, todas avaliadas por scripts/bidmc/run_bidmc_validation.py (fases 2 e 3):
#   pipeline-125  pipeline na taxa nativa da base (125 Hz)
#   pipeline-25   pipeline na taxa que o MAX30102 entrega ao app (25 Hz)
#   processor-25  PpgProcessor do app, também a 25 Hz
#
# Uso: run_comparison.sh <bidmc_prepared_dir> <tcc_binary> <ppg_processor_export> <saida_dir>
#   bidmc_prepared_dir    saída de scripts/bidmc/prepare_bidmc_data.py (+ breath reference)
#   tcc_binary            executável tcc deste projeto (build em Release)
#   ppg_processor_export  executável ppg-processor-export do monitor-news2
set -eu

prepared=$1
tcc_binary=$2
processor_export=$3
out=$4
here=$(cd "$(dirname "$0")" && pwd)
validation="$here/../bidmc/run_bidmc_validation.py"

python3 "$here/prepare_device_rate.py" "$prepared" "$out/bidmc-25hz" --rate 25

run() {
    name=$1; data=$2; binary=$3
    mkdir -p "$out/$name"
    python3 "$validation" --phase 2 --prepared-dir "$data" --tcc-binary "$binary" \
        --out-dir "$out/$name" > "$out/$name/fase2.log" 2>&1
    python3 "$validation" --phase 3 --prepared-dir "$data" --out-dir "$out/$name" \
        --breath-out-dir "$out/$name/anotadores" > "$out/$name/fase3.log" 2>&1
    echo "$name: resultados em $out/$name/fase2.log e fase3.log"
}

run pipeline-125 "$prepared" "$tcc_binary"
run pipeline-25 "$out/bidmc-25hz" "$tcc_binary"
run processor-25 "$out/bidmc-25hz" "$processor_export"
