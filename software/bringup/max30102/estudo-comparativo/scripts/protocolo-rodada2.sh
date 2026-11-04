#!/bin/sh
# Rodada 2 do comparativo: ordem invertida (IIO primeiro) e diagnostico se o IIO vier vazio
killall monitor-news2 2>/dev/null
mount -t debugfs none /sys/kernel/debug 2>/dev/null
mkdir -p /root/comparativo2 && cd /root/comparativo2 && rm -f *.csv *.txt
segment=1
for backend in iio i2cdev iio i2cdev; do
    if [ "$backend" = iio ]; then /etc/init.d/S30max30102 start; else /etc/init.d/S30max30102 stop; fi
    sleep 1
    name="seg${segment}-${backend}"
    echo "$(date +%T) inicio $name INT=$(grep -m1 'gpio-17 ' /sys/kernel/debug/gpio | tr -s ' ')" >> log.txt
    /root/ppg-backend-recorder "$backend" 75 > "$name.csv" 2> "$name-resumo.txt"
    echo "$(date +%T) fim $name $(cat $name-resumo.txt)" >> log.txt
    if [ "$backend" = iio ] && grep -q "amostras=0 " "$name-resumo.txt"; then
        echo "DIAG gpio: $(grep 'gpio-17 ' /sys/kernel/debug/gpio)" >> log.txt
        echo "DIAG irq: $(grep max30102 /proc/interrupts)" >> log.txt
        echo "DIAG status1/2/inten/mode/fifo: $(for r in 0x00 0x01 0x02 0x09 0x08 0x04 0x06; do i2cget -f -y 1 0x57 $r; done | tr '\n' ' ')" >> log.txt
        echo "DIAG gpio apos ler status: $(grep 'gpio-17 ' /sys/kernel/debug/gpio)" >> log.txt
    fi
    segment=$((segment + 1))
done
echo FIM >> log.txt
