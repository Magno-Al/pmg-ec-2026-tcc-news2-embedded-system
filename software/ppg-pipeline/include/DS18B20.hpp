#ifndef DS18B20_HPP
#define DS18B20_HPP

#include <optional>
#include <string>

// Diretório onde o driver w1-therm do kernel expõe cada DS18B20 (família 28) como 28-<id>.
constexpr const char* DS18B20_W1_DEVICES_DIR = "/sys/bus/w1/devices";

// Lê um arquivo no formato w1_slave do driver w1-therm: a primeira linha termina em "YES"
// quando o CRC confere, a segunda traz "t=<milésimos de °C>". nullopt se o arquivo não
// abrir, o CRC falhar ou a temperatura estiver ausente.
std::optional<double> read_ds18b20_w1_slave(const std::string& w1_slave_path);

// Caminho do w1_slave do primeiro DS18B20 no barramento 1-Wire, ou nullopt sem sensor.
std::optional<std::string> find_ds18b20_w1_slave(const std::string& devices_dir = DS18B20_W1_DEVICES_DIR);

// Temperatura do primeiro DS18B20 do barramento; sem sensor, lê o arquivo de exemplo
// ../data/w1_slave_ok.txt (execução fora da Raspberry).
std::optional<double> get_temp();

#endif
