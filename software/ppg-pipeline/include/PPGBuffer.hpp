#ifndef PPG_BUFFER_HPP
#define PPG_BUFFER_HPP

#include <cstddef>
#include <vector>
#include "PPG.hpp"

// Arredonda duration_s * sample_rate_hz pro inteiro mais próximo (não trunca).
size_t seconds_to_samples(double duration_s, int sample_rate_hz);

struct PPGBuffer {
    std::vector<PPGWindowSample> samples;
    size_t count = 0; // amostras válidas na janela (cresce até buffer_capacity no primeiro preenchimento)

    std::vector<PPGWindowSample> pending;
    size_t pending_count = 0; // amostras novas acumuladas desde o último deslocamento da janela

    size_t buffer_capacity = 0;
    size_t hop_samples = 0;
};

// Deve ser chamada uma única vez, logo após open_ppg_source (PPGSource.hpp).
PPGBuffer make_ppg_buffer(int sample_rate_hz);

// Retorna true quando uma janela de PPG_WINDOW_SECONDS_MAX fica pronta em
// buffer.samples (no primeiro preenchimento e depois a cada hop).
bool ppg_buffer_push(PPGBuffer& buffer, const PPGWindowSample& sample);
bool ppg_buffer_is_full(const PPGBuffer& buffer);
void ppg_buffer_clear(PPGBuffer& buffer);

// Fatia somente-leitura dos últimos `window_samples` elementos do buffer, sem copiar.
struct PPGWindowView {
    const PPGWindowSample* data;
    size_t count;

    const PPGWindowSample& operator[](size_t i) const { return data[i]; }
    size_t size() const { return count; }
    const PPGWindowSample* begin() const { return data; }
    const PPGWindowSample* end() const { return data + count; }
};

PPGWindowView last_window(const PPGBuffer& buffer, size_t window_samples);

#endif
