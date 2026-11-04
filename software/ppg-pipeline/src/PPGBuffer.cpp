#include <algorithm>
#include <cmath>
#include "PPGBuffer.hpp"

size_t seconds_to_samples(double duration_s, int sample_rate_hz) {
    return static_cast<size_t>(std::llround(duration_s * sample_rate_hz));
}

PPGBuffer make_ppg_buffer(int sample_rate_hz) {
    PPGBuffer buffer;
    buffer.buffer_capacity = seconds_to_samples(PPG_WINDOW_SECONDS_MAX, sample_rate_hz);
    buffer.hop_samples = seconds_to_samples(PPG_WINDOW_HOP_SECONDS, sample_rate_hz);
    buffer.samples.resize(buffer.buffer_capacity);
    buffer.pending.resize(buffer.hop_samples);
    return buffer;
}

bool ppg_buffer_push(PPGBuffer& buffer, const PPGWindowSample& sample) {
    if (!ppg_buffer_is_full(buffer)) {
        buffer.samples[buffer.count] = sample;
        buffer.count++;
        return ppg_buffer_is_full(buffer); // primeira janela pronta
    }

    buffer.pending[buffer.pending_count] = sample;
    buffer.pending_count++;

    if (buffer.pending_count < buffer.hop_samples) {
        return false;
    }

    // Descarta as hop_samples amostras mais antigas e anexa as novas no fim da janela.
    auto hop = static_cast<std::vector<PPGWindowSample>::difference_type>(buffer.hop_samples);
    std::move(buffer.samples.begin() + hop, buffer.samples.end(), buffer.samples.begin());
    std::move(buffer.pending.begin(), buffer.pending.end(), buffer.samples.end() - hop);
    buffer.pending_count = 0;

    return true; // nova janela pronta
}

bool ppg_buffer_is_full(const PPGBuffer& buffer) {
    return buffer.count >= buffer.buffer_capacity;
}

void ppg_buffer_clear(PPGBuffer& buffer) {
    buffer.count = 0;
    buffer.pending_count = 0;
}

PPGWindowView last_window(const PPGBuffer& buffer, size_t window_samples) {
    return PPGWindowView{buffer.samples.data() + (buffer.samples.size() - window_samples), window_samples};
}
