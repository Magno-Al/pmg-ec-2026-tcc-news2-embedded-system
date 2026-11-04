#!/bin/sh
# Estudo comparativo i2c-dev x IIO: segmentos alternados de 75 s com o dedo no sensor
killall monitor-news2 2>/dev/null
mkdir -p /root/comparativo && cd /root/comparativo && rm -f *.csv *.txt
segment=1
for backend in i2cdev iio i2cdev iio; do
    if [ "$backend" = iio ]; then /etc/init.d/S30max30102 start; else /etc/init.d/S30max30102 stop; fi
    sleep 1
    name="seg${segment}-${backend}"
    echo "$(date +%T) inicio $name" >> log.txt
    /root/ppg-backend-recorder "$backend" 75 > "$name.csv" 2> "$name-resumo.txt"
    echo "$(date +%T) fim $name $(cat $name-resumo.txt)" >> log.txt
    segment=$((segment + 1))
done
echo FIM >> log.txt
