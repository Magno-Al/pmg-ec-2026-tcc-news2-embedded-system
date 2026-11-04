// Teste de bancada do MAX30102 (PPG) pela /dev/i2c-1: le a FIFO por polling,
// detecta os batimentos no canal IR e imprime 1 linha por segundo com a FC.
//
// Uso: max30102-hr-test [segundos] [--raw]

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr const char* I2C_DEVICE = "/dev/i2c-1";
constexpr uint8_t SENSOR_ADDRESS = 0x57;
constexpr uint8_t EXPECTED_PART_ID = 0x15;

constexpr uint8_t REGISTER_FIFO_WRITE_POINTER = 0x04;
constexpr uint8_t REGISTER_FIFO_OVERFLOW_COUNTER = 0x05;
constexpr uint8_t REGISTER_FIFO_READ_POINTER = 0x06;
constexpr uint8_t REGISTER_FIFO_DATA = 0x07;
constexpr uint8_t REGISTER_FIFO_CONFIG = 0x08;
constexpr uint8_t REGISTER_MODE_CONFIG = 0x09;
constexpr uint8_t REGISTER_SPO2_CONFIG = 0x0A;
constexpr uint8_t REGISTER_LED_RED_AMPLITUDE = 0x0C;
constexpr uint8_t REGISTER_LED_IR_AMPLITUDE = 0x0D;
constexpr uint8_t REGISTER_PART_ID = 0xFF;

// 100 amostras/s com media de 4 na FIFO -> 25 amostras/s entregues
constexpr double SAMPLE_RATE_HZ = 25.0;
constexpr uint32_t FINGER_THRESHOLD = 50000;
// 0,3 s entre batimentos -> no maximo 200 bpm
constexpr double MINIMUM_BEAT_INTERVAL_SECONDS = 0.3;
constexpr double MAXIMUM_BEAT_INTERVAL_SECONDS = 2.0;
constexpr size_t AVERAGED_BEAT_COUNT = 5;

class I2cDevice
{
public:
    I2cDevice(const char* path, uint8_t address)
        : m_address(address)
    {
        m_fileDescriptor = open(path, O_RDWR);
        if(m_fileDescriptor < 0)
        {
            throw std::runtime_error(std::string("nao abriu ") + path + ": " + strerror(errno));
        }
    }

    ~I2cDevice()
    {
        close(m_fileDescriptor);
    }

    void writeRegister(uint8_t reg, uint8_t value)
    {
        uint8_t buffer[2] = {reg, value};
        i2c_msg message = {m_address, 0, sizeof(buffer), buffer};
        transfer(&message, 1);
    }

    void readRegisters(uint8_t reg, uint8_t* destination, uint16_t length)
    {
        i2c_msg messages[2] = {
            {m_address, 0, 1, &reg},
            {m_address, I2C_M_RD, length, destination}
        };
        transfer(messages, 2);
    }

    uint8_t readRegister(uint8_t reg)
    {
        uint8_t value = 0;
        readRegisters(reg, &value, 1);
        return value;
    }

private:
    void transfer(i2c_msg* messages, uint32_t count)
    {
        i2c_rdwr_ioctl_data data = {messages, count};
        if(ioctl(m_fileDescriptor, I2C_RDWR, &data) < 0)
        {
            throw std::runtime_error(std::string("falha I2C: ") + strerror(errno));
        }
    }

    int m_fileDescriptor;
    uint8_t m_address;
};

struct Sample
{
    uint32_t red;
    uint32_t infrared;
};

void configureSensor(I2cDevice& sensor)
{
    sensor.writeRegister(REGISTER_MODE_CONFIG, 0x40);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // media de 4 amostras + rollover habilitado
    sensor.writeRegister(REGISTER_FIFO_CONFIG, 0x50);
    // modo SpO2: LED vermelho + IR
    sensor.writeRegister(REGISTER_MODE_CONFIG, 0x03);
    // ADC 4096 nA, 100 amostras/s, pulso de 411 us (18 bits)
    sensor.writeRegister(REGISTER_SPO2_CONFIG, 0x27);
    // ~7 mA em cada LED
    sensor.writeRegister(REGISTER_LED_RED_AMPLITUDE, 0x24);
    sensor.writeRegister(REGISTER_LED_IR_AMPLITUDE, 0x24);
    sensor.writeRegister(REGISTER_FIFO_WRITE_POINTER, 0x00);
    sensor.writeRegister(REGISTER_FIFO_OVERFLOW_COUNTER, 0x00);
    sensor.writeRegister(REGISTER_FIFO_READ_POINTER, 0x00);
}

std::vector<Sample> readAvailableSamples(I2cDevice& sensor)
{
    const uint8_t writePointer = sensor.readRegister(REGISTER_FIFO_WRITE_POINTER);
    const uint8_t readPointer = sensor.readRegister(REGISTER_FIFO_READ_POINTER);
    const int availableCount = (writePointer - readPointer) & 0x1F;

    std::vector<Sample> samples;
    if(availableCount == 0)
    {
        return samples;
    }
    std::vector<uint8_t> raw(availableCount * 6);
    sensor.readRegisters(REGISTER_FIFO_DATA, raw.data(), raw.size());
    for(int index = 0; index < availableCount; ++index)
    {
        const uint8_t* bytes = &raw[index * 6];
        Sample sample;
        sample.red = ((bytes[0] << 16) | (bytes[1] << 8) | bytes[2]) & 0x3FFFF;
        sample.infrared = ((bytes[3] << 16) | (bytes[4] << 8) | bytes[5]) & 0x3FFFF;
        samples.push_back(sample);
    }
    return samples;
}

// Filtro biquad (RBJ Audio EQ Cookbook), forma direta I.
class Biquad
{
public:
    static Biquad highPass(double cutoffHz, double sampleRateHz)
    {
        return design(cutoffHz, sampleRateHz, true);
    }

    static Biquad lowPass(double cutoffHz, double sampleRateHz)
    {
        return design(cutoffHz, sampleRateHz, false);
    }

    double process(double input)
    {
        const double output = m_b0 * input + m_b1 * m_input1 + m_b2 * m_input2
            - m_a1 * m_output1 - m_a2 * m_output2;
        m_input2 = m_input1;
        m_input1 = input;
        m_output2 = m_output1;
        m_output1 = output;
        return output;
    }

    void prime(double input, double output)
    {
        m_input1 = m_input2 = input;
        m_output1 = m_output2 = output;
    }

private:
    static Biquad design(double cutoffHz, double sampleRateHz, bool isHighPass)
    {
        const double omega = 2.0 * M_PI * cutoffHz / sampleRateHz;
        const double alpha = std::sin(omega) / (2.0 * M_SQRT1_2);
        const double cosine = std::cos(omega);
        const double a0 = 1.0 + alpha;
        Biquad filter;
        if(isHighPass)
        {
            filter.m_b0 = (1.0 + cosine) / 2.0 / a0;
            filter.m_b1 = -(1.0 + cosine) / a0;
        }
        else
        {
            filter.m_b0 = (1.0 - cosine) / 2.0 / a0;
            filter.m_b1 = (1.0 - cosine) / a0;
        }
        filter.m_b2 = filter.m_b0;
        filter.m_a1 = -2.0 * cosine / a0;
        filter.m_a2 = (1.0 - alpha) / a0;
        return filter;
    }

    double m_b0 = 1.0;
    double m_b1 = 0.0;
    double m_b2 = 0.0;
    double m_a1 = 0.0;
    double m_a2 = 0.0;
    double m_input1 = 0.0;
    double m_input2 = 0.0;
    double m_output1 = 0.0;
    double m_output2 = 0.0;
};

// Passa-banda de 0,5 a 4 Hz (30 a 240 bpm) no IR invertido -- o sangue da
// sistole absorve mais IR, entao cada batimento e uma queda no sinal bruto.
// Um pico so conta se for o maximo numa janela de +-0,24 s e passar de metade
// da amplitude recente; o ruido na descida lenta do pulso nao passa nesse filtro.
class BeatDetector
{
public:
    // devolve true quando um batimento valido foi detectado nesta amostra
    bool addSample(uint32_t infrared, double& beatsPerMinute)
    {
        if(m_sampleIndex == 0)
        {
            m_highPass.prime(-double(infrared), 0.0);
        }
        ++m_sampleIndex;
        const double filtered = m_lowPass.process(m_highPass.process(-double(infrared)));

        m_window.push_back(filtered);
        if(m_window.size() > 2 * PEAK_HALF_WINDOW + 1)
        {
            m_window.pop_front();
        }
        // o salto do dedo encostando inflaria o limiar por ~20 s: so aprende a
        // amplitude depois que o filtro assentar
        if(m_sampleIndex < WARMUP_SAMPLES)
        {
            return false;
        }
        m_amplitude = std::max(m_amplitude * AMPLITUDE_DECAY, std::fabs(filtered));
        if(m_window.size() < 2 * PEAK_HALF_WINDOW + 1)
        {
            return false;
        }
        const double candidate = m_window[PEAK_HALF_WINDOW];
        for(size_t index = 0; index < m_window.size(); ++index)
        {
            if(index != PEAK_HALF_WINDOW && m_window[index] >= candidate)
            {
                return false;
            }
        }
        if(candidate < 0.5 * m_amplitude)
        {
            return false;
        }

        const long peakIndex = m_sampleIndex - PEAK_HALF_WINDOW;
        const double intervalSeconds = (peakIndex - m_lastBeatIndex) / SAMPLE_RATE_HZ;
        if(m_lastBeatIndex > 0 && intervalSeconds < MINIMUM_BEAT_INTERVAL_SECONDS)
        {
            return false;
        }
        const bool hasPreviousBeat = m_lastBeatIndex > 0;
        m_lastBeatIndex = peakIndex;
        if(!hasPreviousBeat || intervalSeconds > MAXIMUM_BEAT_INTERVAL_SECONDS)
        {
            return false;
        }
        beatsPerMinute = 60.0 / intervalSeconds;
        return true;
    }

    void reset()
    {
        *this = BeatDetector();
    }

private:
    static constexpr size_t PEAK_HALF_WINDOW = 6;
    // transitorio do filtro e do dedo encostando: ignora os 4 primeiros segundos
    static constexpr long WARMUP_SAMPLES = 100;
    // ~0,88 por segundo: acompanha o dedo apertando/aliviando sem perder batimentos
    static constexpr double AMPLITUDE_DECAY = 0.995;

    Biquad m_highPass = Biquad::highPass(0.5, SAMPLE_RATE_HZ);
    Biquad m_lowPass = Biquad::lowPass(4.0, SAMPLE_RATE_HZ);
    std::deque<double> m_window;
    long m_sampleIndex = 0;
    long m_lastBeatIndex = 0;
    double m_amplitude = 0.0;
};

double median(std::deque<double> values)
{
    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}

std::string currentTime()
{
    char text[16];
    const time_t now = time(nullptr);
    strftime(text, sizeof(text), "%H:%M:%S", localtime(&now));
    return text;
}

}

int main(int argc, char* argv[])
{
    int durationSeconds = 60;
    bool shouldPrintRaw = false;
    for(int index = 1; index < argc; ++index)
    {
        if(std::string(argv[index]) == "--raw")
        {
            shouldPrintRaw = true;
        }
        else
        {
            durationSeconds = std::atoi(argv[index]);
        }
    }

    try
    {
        I2cDevice sensor(I2C_DEVICE, SENSOR_ADDRESS);
        const uint8_t partId = sensor.readRegister(REGISTER_PART_ID);
        std::printf("MAX30102 em 0x%02X, part id 0x%02X %s\n", SENSOR_ADDRESS, partId,
            partId == EXPECTED_PART_ID ? "(ok)" : "(INESPERADO)");
        configureSensor(sensor);
        std::printf("lendo por %d s a %.0f amostras/s; posicione o dedo e fique parado\n",
            durationSeconds, SAMPLE_RATE_HZ);

        BeatDetector detector;
        std::deque<double> recentBeats;
        std::vector<double> beatsThisSecond;
        uint64_t infraredSum = 0;
        uint64_t redSum = 0;
        int sampleCountThisSecond = 0;
        bool isFingerPresent = false;

        const auto start = std::chrono::steady_clock::now();
        auto nextPrint = start + std::chrono::seconds(1);
        while(std::chrono::steady_clock::now() - start < std::chrono::seconds(durationSeconds))
        {
            for(const Sample& sample : readAvailableSamples(sensor))
            {
                if(shouldPrintRaw)
                {
                    std::printf("%u,%u\n", sample.infrared, sample.red);
                }
                infraredSum += sample.infrared;
                redSum += sample.red;
                ++sampleCountThisSecond;

                const bool hasFinger = sample.infrared > FINGER_THRESHOLD;
                if(hasFinger != isFingerPresent)
                {
                    detector.reset();
                    recentBeats.clear();
                    isFingerPresent = hasFinger;
                }
                double beatsPerMinute = 0.0;
                if(hasFinger && detector.addSample(sample.infrared, beatsPerMinute))
                {
                    beatsThisSecond.push_back(beatsPerMinute);
                    recentBeats.push_back(beatsPerMinute);
                    if(recentBeats.size() > AVERAGED_BEAT_COUNT)
                    {
                        recentBeats.pop_front();
                    }
                }
            }

            if(!shouldPrintRaw && std::chrono::steady_clock::now() >= nextPrint)
            {
                nextPrint += std::chrono::seconds(1);
                const unsigned long infraredMean = sampleCountThisSecond
                    ? infraredSum / sampleCountThisSecond : 0;
                const unsigned long redMean = sampleCountThisSecond
                    ? redSum / sampleCountThisSecond : 0;
                if(!isFingerPresent)
                {
                    std::printf("%s  dedo nao detectado (IR %lu)\n", currentTime().c_str(),
                        infraredMean);
                }
                else if(recentBeats.empty())
                {
                    std::printf("%s  detectando pulso... (IR %lu, Red %lu)\n",
                        currentTime().c_str(), infraredMean, redMean);
                }
                else
                {
                    std::printf("%s  FC: %5.1f bpm (mediana de %zu batimentos, %zu neste segundo)"
                        " | IR %lu Red %lu\n", currentTime().c_str(), median(recentBeats),
                        recentBeats.size(), beatsThisSecond.size(), infraredMean, redMean);
                }
                std::fflush(stdout);
                beatsThisSecond.clear();
                infraredSum = 0;
                redSum = 0;
                sampleCountThisSecond = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        sensor.writeRegister(REGISTER_MODE_CONFIG, 0x80);
    }
    catch(const std::exception& error)
    {
        std::fprintf(stderr, "ERRO: %s\n", error.what());
        return 1;
    }
    return 0;
}
