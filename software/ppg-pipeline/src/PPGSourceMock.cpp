#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include "PPGSource.hpp"

namespace {

constexpr const char* RED_SIGNAL_HEADER = "red_signal";
constexpr const char* IR_SIGNAL_HEADER = "ir_signal";
constexpr const char* DEFAULT_MOCK_FILE =
    "../data/openoximetry/prepared/01a542fef90c977867f1016bb46772e233006183c5c879b4c3adb77445aa1969_ppg_prepared.csv";

std::ifstream ppg_file;
bool ppg_file_opened = false;
int red_signal_column = -1;
int ir_signal_column = -1;
bool has_red_channel = false;

// Posição das colunas descoberta pelo cabeçalho; red_signal é opcional,
// ir_signal é obrigatório (ver README, seção 4).
bool find_signal_columns(const std::string& header) {
    std::stringstream header_stream(header);
    std::string column_name;

    for (int column = 0; std::getline(header_stream, column_name, ','); column++) {
        if (column_name == RED_SIGNAL_HEADER) {
            red_signal_column = column;
        } else if (column_name == IR_SIGNAL_HEADER) {
            ir_signal_column = column;
        }
    }

    has_red_channel = red_signal_column != -1;
    return ir_signal_column != -1;
}

// Mesmo caminho do CSV, com a extensão trocada por .meta.
std::string derive_meta_path(const std::string& csv_path) {
    size_t dot = csv_path.find_last_of('.');
    std::string base = (dot == std::string::npos) ? csv_path : csv_path.substr(0, dot);
    return base + ".meta";
}

// nullopt se o arquivo não existir ou a chave não estiver presente.
std::optional<std::string> read_meta_value(const std::string& meta_path, const std::string& key) {
    std::ifstream meta_file(meta_path);
    if (!meta_file.is_open()) {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(meta_file, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        if (line.substr(0, eq) == key) {
            return line.substr(eq + 1);
        }
    }
    return std::nullopt;
}

// 0 se a chave sample_rate_hz estiver ausente/inválida.
int read_sample_rate_from_meta(const std::string& meta_path) {
    auto value = read_meta_value(meta_path, "sample_rate_hz");
    return value ? std::atoi(value->c_str()) : 0;
}

// Default true; false só se a chave valer "false" explicitamente.
bool read_raw_intensity_from_meta(const std::string& meta_path) {
    auto value = read_meta_value(meta_path, "raw_intensity");
    return !value || *value != "false";
}

bool open_ppg_file() {
    const char* path_override = std::getenv("PPG_MOCK_FILE");
    std::string csv_path = path_override ? path_override : DEFAULT_MOCK_FILE;

    ppg_file.open(csv_path);
    if (!ppg_file.is_open()) {
        return false;
    }

    std::string header;
    std::getline(ppg_file, header);
    if (!find_signal_columns(header)) {
        return false;
    }

    return true;
}

} // namespace

PPGSourceInfo open_ppg_source() {
    if (!ppg_file_opened) {
        ppg_file_opened = true;
        if (!open_ppg_file()) {
            std::cerr << "PPGSourceMock: nao foi possivel abrir a fonte ou encontrar a coluna "
                      << IR_SIGNAL_HEADER << "\n";
            return PPGSourceInfo{0, false};
        }
    }

    const char* path_override = std::getenv("PPG_MOCK_FILE");
    std::string csv_path = path_override ? path_override : DEFAULT_MOCK_FILE;
    std::string meta_path = derive_meta_path(csv_path);
    int sample_rate_hz = read_sample_rate_from_meta(meta_path);
    if (sample_rate_hz <= 0) {
        std::cerr << "PPGSourceMock: taxa de amostragem ausente/invalida em " << meta_path
                  << " (esperado sample_rate_hz=<N>)\n";
        return PPGSourceInfo{0, has_red_channel};
    }

    bool raw_intensity = read_raw_intensity_from_meta(meta_path);
    return PPGSourceInfo{sample_rate_hz, has_red_channel, raw_intensity};
}

std::optional<PPGSample> read_ppg_sample() {
    if (!ppg_file_opened) {
        ppg_file_opened = true;
        if (!open_ppg_file()) {
            return std::nullopt;
        }
    }

    std::string line;
    if (!std::getline(ppg_file, line)) {
        return std::nullopt; // fim do arquivo
    }

    std::stringstream line_stream(line);
    std::string field;
    std::optional<double> red;
    std::optional<double> ir;

    for (int column = 0; std::getline(line_stream, field, ','); column++) {
        if (column == red_signal_column) {
            red = std::stod(field);
        } else if (column == ir_signal_column) {
            ir = std::stod(field);
        }
    }

    if (!ir) {
        return std::nullopt;
    }

    PPGSample sample;
    sample.red = red ? static_cast<uint32_t>(*red) : 0;
    sample.ir = static_cast<uint32_t>(*ir);
    return sample;
}
