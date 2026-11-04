// Roda o PpgProcessor do app sobre um CSV preparado pelos scripts de validação do
// ppg-pipeline e exporta FC e FR por janela no mesmo formato do pipeline, para que
// software/ppg-pipeline/scripts/bidmc/run_bidmc_validation.py avalie os dois métodos com o
// mesmo protocolo (passe este executável em --tcc-binary).
//
// Entrada (variáveis de ambiente, como no pipeline):
//   PPG_MOCK_FILE          CSV com a coluna ir_signal (red_signal opcional) + .meta ao lado
//   PPG_PARAM_EXPORT_PATH  CSV de saída
//
// O PpgProcessor trabalha a 25 Hz, a taxa que o MAX30102 entrega ao app; uma entrada com taxa
// múltipla de 25 é reduzida por média de blocos, como a média de amostras do próprio sensor.

#include "core/PpgProcessor.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// Bases públicas não trazem intensidade bruta do fotodetector (raw_intensity=false) e ficam
// abaixo do limiar de contato do app; um nível DC fixo faz o registro inteiro valer como
// dedo presente. Não muda frequência nem forma do pulso.
constexpr double ContactLevelOffset = 100000.0;
constexpr int ProcessorRateHz = 25;
constexpr int HopSeconds = 5;
constexpr int HeartRateWindowSeconds = 30;
constexpr int RespiratoryWindowSeconds = 60;

std::optional<std::string> metaValue(const std::string& metaPath, const std::string& key)
{
    std::ifstream meta(metaPath);
    std::string line;
    while(std::getline(meta, line))
    {
        const size_t equals = line.find('=');
        if(equals != std::string::npos && line.substr(0, equals) == key)
        {
            return line.substr(equals + 1);
        }
    }
    return std::nullopt;
}

std::vector<std::string> splitCsv(const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while(std::getline(stream, field, ','))
    {
        fields.push_back(field);
    }
    return fields;
}

void writeOptional(std::ofstream& file, std::optional<int> value)
{
    if(value)
    {
        file << *value;
    }
}

}

int main()
{
    const char* inputPath = std::getenv("PPG_MOCK_FILE");
    const char* outputPath = std::getenv("PPG_PARAM_EXPORT_PATH");
    if(!inputPath || !outputPath)
    {
        std::fprintf(stderr, "defina PPG_MOCK_FILE e PPG_PARAM_EXPORT_PATH\n");
        return 2;
    }
    const std::string csvPath = inputPath;
    const std::string metaPath = csvPath.substr(0, csvPath.find_last_of('.')) + ".meta";
    const int sourceRateHz = std::atoi(metaValue(metaPath, "sample_rate_hz").value_or("0").c_str());
    if(sourceRateHz <= 0 || sourceRateHz % ProcessorRateHz != 0)
    {
        std::fprintf(stderr, "sample_rate_hz=%d: precisa ser múltiplo de %d\n", sourceRateHz,
                     ProcessorRateHz);
        return 1;
    }
    const int blockSize = sourceRateHz / ProcessorRateHz;

    std::ifstream input(csvPath);
    std::string line;
    std::getline(input, line);
    const std::vector<std::string> header = splitCsv(line);
    int infraredColumn = -1;
    int redColumn = -1;
    for(size_t column = 0; column < header.size(); ++column)
    {
        if(header[column] == "ir_signal")
        {
            infraredColumn = int(column);
        }
        else if(header[column] == "red_signal")
        {
            redColumn = int(column);
        }
    }
    if(infraredColumn < 0)
    {
        std::fprintf(stderr, "coluna ir_signal ausente em %s\n", inputPath);
        return 1;
    }

    std::ofstream output(outputPath);
    output << "start_time_s,end_time_s,hr_status,hr_value,hr_peak_count,spo2_status,spo2_value,"
              "rr_status,rr_value,rr_amplitude_status,rr_amplitude_value,"
              "rr_frequency_status,rr_frequency_value,rr_baseline_status,rr_baseline_value,"
              "pi_red_30s,pi_ir_30s,pi_ir_60s\n";

    PpgProcessor processor;
    double infraredSum = 0.0;
    double redSum = 0.0;
    int blockCount = 0;
    long processedSamples = 0;
    while(std::getline(input, line))
    {
        const std::vector<std::string> fields = splitCsv(line);
        if(int(fields.size()) <= infraredColumn || fields[infraredColumn].empty())
        {
            continue;
        }
        const double infrared = std::stod(fields[infraredColumn]);
        const bool hasRed = redColumn >= 0 && int(fields.size()) > redColumn
            && !fields[redColumn].empty();
        infraredSum += infrared;
        redSum += hasRed ? std::stod(fields[redColumn]) : infrared;
        if(++blockCount < blockSize)
        {
            continue;
        }

        PpgSample sample;
        sample.infrared = uint32_t(std::lround(infraredSum / blockSize + ContactLevelOffset));
        sample.red = uint32_t(std::lround(redSum / blockSize + ContactLevelOffset));
        infraredSum = 0.0;
        redSum = 0.0;
        blockCount = 0;
        processor.addSample(sample);
        ++processedSamples;

        // mesma grade de janelas do pipeline: primeira saída com 60 s de sinal, depois a cada 5 s
        const bool isWindowEnd = processedSamples >= RespiratoryWindowSeconds * ProcessorRateHz
            && processedSamples % (HopSeconds * ProcessorRateHz) == 0;
        if(!isWindowEnd)
        {
            continue;
        }
        const double endSeconds = double(processedSamples) / ProcessorRateHz;
        const std::optional<int> heartRate = processor.heartRate();
        const std::optional<int> respiratoryRate = processor.respiratoryRate();
        output << endSeconds - HeartRateWindowSeconds << ',' << endSeconds << ','
               << (heartRate ? "aceita" : "aceita_sem_fc") << ',';
        writeOptional(output, heartRate);
        output << ",,descartada,,"
               << (respiratoryRate ? "aceita" : "estimativa_ausente") << ',';
        writeOptional(output, respiratoryRate);
        output << ",descartada,,descartada,,descartada,,,,\n";
    }
    return 0;
}
