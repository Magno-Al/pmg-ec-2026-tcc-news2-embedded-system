#include <filesystem>
#include "PPGDebug.hpp"

std::ofstream open_ppg_debug_dump(const std::string& path) {
    std::filesystem::path file_path(path);
    std::filesystem::create_directories(file_path.parent_path());

    std::ofstream file(file_path);
    file << "index,red_raw,red_filtered,ir_raw,ir_filtered\n";
    return file;
}

void write_ppg_debug_sample(std::ofstream& file, size_t index,
                             uint32_t red_raw, double red_filtered,
                             uint32_t ir_raw, double ir_filtered) {
    file << index << ',' << red_raw << ',' << red_filtered << ',' << ir_raw << ',' << ir_filtered << '\n';
}

std::ofstream open_ppg_window_dump(const std::string& path) {
    std::filesystem::path file_path(path);
    std::filesystem::create_directories(file_path.parent_path());

    std::ofstream file(file_path);
    file << "window_index,start_sample,end_sample,accepted\n";
    return file;
}

void write_ppg_window_record(std::ofstream& file, int window_index,
                              size_t start_sample, size_t end_sample, bool accepted) {
    file << window_index << ',' << start_sample << ',' << end_sample << ',' << (accepted ? 1 : 0) << '\n';
}

std::ofstream open_ppg_parameters_export(const std::string& path) {
    std::filesystem::path file_path(path);
    std::filesystem::create_directories(file_path.parent_path());

    std::ofstream file(file_path);
    file << "start_time_s,end_time_s,hr_status,hr_value,hr_peak_count,spo2_status,spo2_value,"
            "rr_status,rr_value,rr_amplitude_status,rr_amplitude_value,"
            "rr_frequency_status,rr_frequency_value,rr_baseline_status,rr_baseline_value,"
            "pi_red_30s,pi_ir_30s,pi_ir_60s\n";
    return file;
}

namespace {

void write_optional(std::ofstream& file, std::optional<double> value) {
    if (value) {
        file << *value;
    }
}

} // namespace

void write_ppg_parameters_record(std::ofstream& file, const PPGParameterRecord& record) {
    file << record.start_time_s << ',' << record.end_time_s << ',' << record.hr_status << ',';
    if (record.hr_value) {
        file << *record.hr_value;
    }
    file << ',';
    if (record.hr_peak_count) {
        file << *record.hr_peak_count;
    }
    file << ',' << record.spo2_status << ',';
    if (record.spo2_value) {
        file << *record.spo2_value;
    }
    file << ',' << record.rr_status << ',';
    write_optional(file, record.rr_value);
    file << ',' << record.rr_amplitude_status << ',';
    write_optional(file, record.rr_amplitude_value);
    file << ',' << record.rr_frequency_status << ',';
    write_optional(file, record.rr_frequency_value);
    file << ',' << record.rr_baseline_status << ',';
    write_optional(file, record.rr_baseline_value);
    file << ',' << record.pi_red_30s << ',' << record.pi_ir_30s << ',' << record.pi_ir_60s;
    file << '\n';
}
