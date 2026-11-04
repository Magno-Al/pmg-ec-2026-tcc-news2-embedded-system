#!/bin/sh
set -eu
TARGET_DIR="$1"
mkdir -p "${TARGET_DIR}/var/lib/news2" "${TARGET_DIR}/var/log"
grep -q 'ttyAMA0' "${TARGET_DIR}/etc/inittab" 2>/dev/null || \
    echo "ttyAMA0::respawn:/sbin/getty -L ttyAMA0 115200 vt100" >> "${TARGET_DIR}/etc/inittab"

# ---- DESENVOLVIMENTO: remover antes da imagem final -------------------------
# Login no HDMI disputa a tela com o app eglfs; root via SSH com senha nao e
# aceitavel num dispositivo medico.
grep -q '^tty1::' "${TARGET_DIR}/etc/inittab" || \
    echo "tty1::respawn:/sbin/getty -L tty1 0 linux" >> "${TARGET_DIR}/etc/inittab"
sed -i 's/^#\?PermitRootLogin .*/PermitRootLogin yes/' "${TARGET_DIR}/etc/ssh/sshd_config"
