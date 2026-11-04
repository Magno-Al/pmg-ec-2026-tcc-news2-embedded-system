#!/usr/bin/env bash
set -euo pipefail
IMG="${1:?uso: flash-sd.sh <img> </dev/sdX>}"; DEV="${2:?}"
[[ -b "$DEV" ]] || { echo "nao e dispositivo de bloco"; exit 1; }
[[ "$DEV" =~ [0-9]$ ]] && { echo "ERRO: parece PARTICAO. Use o disco inteiro."; exit 1; }
[[ "$(lsblk -dno RM "$DEV")" == "1" ]] || { echo "ERRO: $DEV nao e removivel."; exit 1; }
ROOTDISK=$(lsblk -no PKNAME "$(findmnt -no SOURCE /)" 2>/dev/null || true)
[[ -n "$ROOTDISK" && "$DEV" == *"$ROOTDISK"* ]] && { echo "ERRO: contem a raiz!"; exit 1; }
lsblk -o NAME,SIZE,RM,TYPE,MOUNTPOINT,MODEL "$DEV"
read -rp "Digite exatamente 'APAGAR $DEV': " CONFIRMATION
[[ "$CONFIRMATION" == "APAGAR $DEV" ]] || exit 1
for PARTITION in $(lsblk -lno NAME "$DEV" | tail -n +2); do
    sudo umount "/dev/$PARTITION" 2>/dev/null || true
done
sudo dd if="$IMG" of="$DEV" bs=4M conv=fsync oflag=direct status=progress && sudo sync
