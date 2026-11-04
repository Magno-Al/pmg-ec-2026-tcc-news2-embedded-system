import sys, glob, re, os, subprocess, numpy as np
from scipy import signal
DIRS = sys.argv[1].split(","); REPLAY = sys.argv[2]; FS = 25.0; SKIP = 8.0
rows = []
for path in sorted(p for D in DIRS for p in glob.glob(f"{D}/seg*-*.csv")):
    D = os.path.dirname(path)
    name = os.path.basename(path)[:-4]; backend = name.split("-")[1]
    name = os.path.basename(D)[-1:].replace("o", "1") + "." + name
    summary = dict(kv.split("=") for kv in open(path[:-4] + "-resumo.txt").read().split())
    if summary["amostras"] == "0":
        print(f"# {name}: SEM AMOSTRAS (descartado)"); continue
    summary = {k: float(v) for k, v in summary.items()}
    d = np.loadtxt(path, delimiter=",", skiprows=1)
    readAt, batch, red, ir = d[:,0], d[:,1], d[:,2].astype(float), d[:,3].astype(float)
    k = np.arange(len(ir))
    # idade da amostra: tempo de leitura menos o instante nominal pelo relogio do chip
    rate = np.polyfit(k, readAt, 1)[0]; nominal = k * rate
    age = readAt - nominal; age -= age.min()
    keep = k / FS >= SKIP
    irk, redk = ir[keep], red[keep]
    b, a = signal.butter(2, [0.5, 4], btype="band", fs=FS)
    cardIr = signal.filtfilt(b, a, irk); cardRed = signal.filtfilt(b, a, redk)
    bh, ah = signal.butter(4, 5, btype="high", fs=FS)
    noiseIr = signal.filtfilt(bh, ah, irk)
    replay = np.loadtxt(subprocess.run([REPLAY, path], capture_output=True, text=True).stdout.splitlines(), delimiter=",")
    valid = lambda col: replay[(replay[:,0] >= SKIP) & (replay[:,col] > 0), col]
    hr, spo2, rr = valid(2), valid(3), valid(4)
    rows.append(dict(
        segmento=name, backend=backend,
        taxa_hz=summary["taxa_hz"], amostras_esperadas=round(summary["duracao_s"] * 25),
        amostras=int(summary["amostras"]),
        acordadas_s=summary["leituras"] / summary["duracao_s"],
        vazias_pct=100 * summary["leituras_vazias"] / summary["leituras"],
        rajada_media=np.mean(batch), idade_media_ms=1000 * age.mean(), idade_p95_ms=1000 * np.percentile(age, 95),
        idade_max_ms=1000 * age.max(),
        cpu_pct=100 * (summary["cpu_user_s"] + summary["cpu_sys_s"]) / summary["duracao_s"],
        cpu_sys_pct=100 * summary["cpu_sys_s"] / summary["duracao_s"],
        trocas_s=(summary["trocas_voluntarias"] + summary["trocas_involuntarias"]) / summary["duracao_s"],
        dc_ir=irk.mean(), dc_red=redk.mean(),
        ac_ir=np.std(cardIr), pi_ir_pct=100 * (np.percentile(cardIr, 97.5) - np.percentile(cardIr, 2.5)) / irk.mean(),
        ruido_ir=np.std(noiseIr), snr_db=20 * np.log10(np.std(cardIr) / np.std(noiseIr)),
        fc=(hr.mean(), hr.std(), len(hr)), spo2=(spo2.mean(), spo2.std(), len(spo2)),
        fr=(rr.mean() if len(rr) else 0, rr.std() if len(rr) else 0, len(rr)),
    ))
def fmt(v):
    if isinstance(v, tuple): return "%.1f ± %.1f (n=%d)" % v
    if isinstance(v, float): return "%.3g" % v if abs(v) < 100 else "%.0f" % v
    return str(v)
keys = [k for k in rows[0] if k not in ("segmento", "backend")]
print("| métrica | " + " | ".join(r["segmento"] for r in rows) + " |")
print("|---|" + "---|" * len(rows))
for key in keys:
    print("| %s | %s |" % (key, " | ".join(fmt(r[key]) for r in rows)))
