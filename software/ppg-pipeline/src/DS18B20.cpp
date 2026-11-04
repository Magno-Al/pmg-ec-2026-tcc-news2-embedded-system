#include <string>
#include <fstream>
#include <optional>
#include "DS18B20.hpp"

std::optional<double> get_temp() {
    std::string line;
    std::ifstream file("../data/w1_slave_ok.txt");

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