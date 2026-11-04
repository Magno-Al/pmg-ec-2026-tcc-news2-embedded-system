''''
Este script é responsável por obter os arquivos usados para aferiação dos dados dos pacientes
dentro da pasta raw, e juntar tudo em um único arquivo, para facilitar e agilizar a leitura destes,
na etapa de pré-processamento
'''

from pathlib import Path
import pandas as pd

RAW = Path(__file__).resolve().parent.parent / "raw"
OUT = Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)


def detectar_origem(caminho: Path) -> str:
    p = str(caminho).lower()
    if "seta" in p:
        return "A"
    if "setb" in p:
        return "B"
    return "?"


frames = []
arquivos = sorted(RAW.rglob("*.psv"))
print(f"Lendo {len(arquivos)} arquivos...")

for i, fp in enumerate(arquivos):
    df = pd.read_csv(fp, sep="|")
    df["patient_id"] = fp.stem           # ex.: 'p000001' (vem do nome do arquivo)
    df["source"] = detectar_origem(fp)   # 'A' ou 'B'
    df["hour_idx"] = range(len(df))    # índice temporal posicional (ordem = horas)
    frames.append(df)
    if (i + 1) % 5000 == 0:
        print(f"{i+1} lidos...")


full = pd.concat(frames, ignore_index=True)
full.to_csv(OUT / "patientsData.csv", index=False)

print("\n--- Consolidação concluída ---")
print("Shape:", full.shape)
print("Pacientes:", full["patient_id"].nunique())
print("Por origem:")
print(full.groupby("source")["patient_id"].nunique())