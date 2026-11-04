import csv
import os
import sys

import matplotlib
import numpy as np
from matplotlib.patches import Patch

matplotlib.use("Agg")
import matplotlib.pyplot as plt

SAMPLE_RATE_HZ = 86
HOP_DURATION_S = 5.0
ZOOM_DURATION_S = 15.0
OVERVIEW_BINS = 2000

CONTEXT_NOTE = (
    "Sinal PPG (LED vermelho e infravermelho) bruto vs. filtrado (passa-banda 0.5-4 Hz).\n"
    "Etapa de pre-processamento antes do calculo de FC, SpO2 e frequencia respiratoria."
)


def load_csv(path):
    red_raw, red_filtered, ir_raw, ir_filtered = [], [], [], []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            red_raw.append(float(row["red_raw"]))
            red_filtered.append(float(row["red_filtered"]))
            ir_raw.append(float(row["ir_raw"]))
            ir_filtered.append(float(row["ir_filtered"]))
    return (
        np.array(red_raw),
        np.array(red_filtered),
        np.array(ir_raw),
        np.array(ir_filtered),
    )


def derive_windows_path(trace_path):
    candidate = trace_path.replace("full_trace", "windows")
    if candidate != trace_path and os.path.exists(candidate):
        return candidate
    return None


def load_window_verdicts(path):
    hop_samples = int(round(HOP_DURATION_S * SAMPLE_RATE_HZ))
    regions = []  # (start_s, end_s, accepted)
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            window_index = int(row["window_index"])
            end_sample = int(row["end_sample"])
            accepted = row["accepted"] == "1"

            if window_index == 1:
                region_start_sample = int(row["start_sample"])
            else:
                region_start_sample = end_sample - hop_samples + 1

            regions.append((region_start_sample / SAMPLE_RATE_HZ, (end_sample + 1) / SAMPLE_RATE_HZ, accepted))
    return regions


def shade_windows(ax, regions):
    for start_s, end_s, accepted in regions:
        color = "tab:green" if accepted else "tab:red"
        ax.axvspan(start_s, end_s, color=color, alpha=0.15, linewidth=0)


WINDOW_LEGEND = [
    Patch(facecolor="tab:green", alpha=0.3, label="janela aceita"),
    Patch(facecolor="tab:red", alpha=0.3, label="janela descartada"),
]


def plot_overview(path, out_path, red_raw, red_filtered, ir_raw, ir_filtered, window_regions):
    n = len(red_raw)
    duration_s = n / SAMPLE_RATE_HZ
    bin_size = max(1, n // OVERVIEW_BINS)

    red_raw_min, red_raw_max = envelope(red_raw, bin_size)
    red_filt_min, red_filt_max = envelope(red_filtered, bin_size)
    ir_raw_min, ir_raw_max = envelope(ir_raw, bin_size)
    ir_filt_min, ir_filt_max = envelope(ir_filtered, bin_size)
    bin_time = np.arange(len(red_raw_min)) * bin_size / SAMPLE_RATE_HZ

    fig, axes = plt.subplots(2, 2, figsize=(13, 7))
    fig.suptitle(f"{path}\nvisão geral ({duration_s:.0f}s) — {CONTEXT_NOTE}")

    axes[0][0].fill_between(bin_time, red_raw_min, red_raw_max, color="tab:red", alpha=0.7)
    axes[0][0].set_title("Red - bruto")

    axes[0][1].fill_between(bin_time, red_filt_min, red_filt_max, color="tab:red", alpha=0.7)
    axes[0][1].set_title("Red - filtrado (0.5-4 Hz)")

    axes[1][0].fill_between(bin_time, ir_raw_min, ir_raw_max, color="tab:gray", alpha=0.7)
    axes[1][0].set_title("IR - bruto")

    axes[1][1].fill_between(bin_time, ir_filt_min, ir_filt_max, color="tab:gray", alpha=0.7)
    axes[1][1].set_title("IR - filtrado (0.5-4 Hz)")

    for row in axes:
        for ax in row:
            ax.set_xlabel("tempo (s)")
            ax.grid(True, alpha=0.3)
            if window_regions is not None:
                shade_windows(ax, window_regions)

    if window_regions is not None:
        fig.legend(handles=WINDOW_LEGEND, loc="upper right", ncol=2)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"salvo em {out_path}")


def plot_zoom(path, out_path, zoom_start_s, red_raw, red_filtered, ir_raw, ir_filtered):
    n = len(red_raw)
    zoom_start = min(int(zoom_start_s * SAMPLE_RATE_HZ), max(0, n - 1))
    zoom_end = min(zoom_start + int(ZOOM_DURATION_S * SAMPLE_RATE_HZ), n)
    zoom_time = np.arange(zoom_start, zoom_end) / SAMPLE_RATE_HZ

    fig, axes = plt.subplots(2, 2, figsize=(13, 7), sharex=True)
    fig.suptitle(f"{path}\nzoom em t={zoom_start_s:.0f}s ({ZOOM_DURATION_S:.0f}s) — {CONTEXT_NOTE}")

    axes[0][0].plot(zoom_time, red_raw[zoom_start:zoom_end], color="tab:red")
    axes[0][0].set_title("Red - bruto")

    axes[0][1].plot(zoom_time, red_filtered[zoom_start:zoom_end], color="tab:red")
    axes[0][1].set_title("Red - filtrado (0.5-4 Hz)")

    axes[1][0].plot(zoom_time, ir_raw[zoom_start:zoom_end], color="tab:gray")
    axes[1][0].set_title("IR - bruto")

    axes[1][1].plot(zoom_time, ir_filtered[zoom_start:zoom_end], color="tab:gray")
    axes[1][1].set_title("IR - filtrado (0.5-4 Hz)")

    for row in axes:
        for ax in row:
            ax.set_xlabel("tempo (s)")
            ax.grid(True, alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"salvo em {out_path}")


def envelope(signal, bin_size):
    n_bins = len(signal) // bin_size
    trimmed = signal[: n_bins * bin_size].reshape(n_bins, bin_size)
    return trimmed.min(axis=1), trimmed.max(axis=1)


def main():
    if len(sys.argv) < 2:
        print("uso: plot_ppg_overview.py <csv_de_debug> [prefixo_saida] [zoom_inicio_s] [csv_de_janelas]")
        sys.exit(1)

    path = sys.argv[1]
    out_prefix = sys.argv[2] if len(sys.argv) > 2 else "ppg"
    zoom_start_s = float(sys.argv[3]) if len(sys.argv) > 3 else 60.0
    windows_path = sys.argv[4] if len(sys.argv) > 4 else derive_windows_path(path)

    red_raw, red_filtered, ir_raw, ir_filtered = load_csv(path)
    window_regions = load_window_verdicts(windows_path) if windows_path else None
    if windows_path:
        print(f"usando veredito de qualidade de {windows_path}")

    plot_overview(path, f"{out_prefix}_overview.png", red_raw, red_filtered, ir_raw, ir_filtered, window_regions)
    plot_zoom(path, f"{out_prefix}_zoom.png", zoom_start_s, red_raw, red_filtered, ir_raw, ir_filtered)


if __name__ == "__main__":
    main()
