#include <dirent.h>
#include <fstream>
#include <optional>
#include <string>
#include "DS18B20.hpp"

namespace {

constexpr const char* DS18B20_FAMILY_PREFIX = "28-";
constexpr const char* SAMPLE_W1_SLAVE_FILE = "../data/w1_slave_ok.txt";

}

std::optional<double> read_ds18b20_w1_slave(const std::string& w1_slave_path) {
    std::string line;
    std::ifstream file(w1_slave_path);

    if (!file.is_open()) {
        return std::nullopt;
    }

    std::getline(file, line);
    if (line.find("YES") == std::string::npos) {
        return std::nullopt;
    }

    std::getline(file, line);
    auto pos = line.find("t=");

    if (pos == std::string::npos) {
        return std::nullopt;
    }

    int milicelsius = std::stoi(line.substr(pos + 2));

    file.close();

    return milicelsius / 1000.0;
}

std::optional<std::string> find_ds18b20_w1_slave(const std::string& devices_dir) {
    DIR* directory = opendir(devices_dir.c_str());
    if (directory == nullptr) {
        return std::nullopt;
    }

    std::optional<std::string> w1_slave_path;
    while (dirent* entry = readdir(directory)) {
        std::string name = entry->d_name;
        if (name.rfind(DS18B20_FAMILY_PREFIX, 0) == 0) {
            w1_slave_path = devices_dir + "/" + name + "/w1_slave";
            break;
        }
    }
    closedir(directory);
    return w1_slave_path;
}

std::optional<double> get_temp() {
    if (auto w1_slave_path = find_ds18b20_w1_slave()) {
        return read_ds18b20_w1_slave(*w1_slave_path);
    }
    return read_ds18b20_w1_slave(SAMPLE_W1_SLAVE_FILE);
}
