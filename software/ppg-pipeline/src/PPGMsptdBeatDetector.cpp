// Ver licença, referências e o que NÃO foi portado em PPGMsptdBeatDetector.hpp.
//
// Conversão de índices do MATLAB original (1-based, intervalos inclusivos nos
// dois extremos) para C++ (0-based): um índice 1-based `i` corresponde ao índice
// 0-based `i-1`; um intervalo MATLAB `a:b` (inclusive nos dois extremos)
// corresponde ao intervalo 0-based `[a-1, b-1]`, também inclusive nos dois
// extremos (não `[a-1, b-1)` como um end-exclusive do C++ costumaria usar).
// Cada trecho abaixo comenta a conversão específica feita ali.
#include <algorithm>
#include <cmath>
#include "PPGMsptdBeatDetector.hpp"

namespace {

// Equivalente a detrend(x) do MATLAB sem segundo argumento: remove a reta de
// mínimos quadrados (grau 1) ajustada a x, usando o índice da amostra (0-based)
// como variável independente -- o deslocamento absoluto do eixo de tempo não
// muda o resultado da regressão linear em relação à versão 1-based do MATLAB.
std::vector<double> detrend_linear(const std::vector<double>& x) {
    size_t n = x.size();
    if (n < 2) {
        return x;
    }

    double t_mean = (static_cast<double>(n) - 1.0) / 2.0;
    double x_mean = 0.0;
    for (double v : x) {
        x_mean += v;
    }
    x_mean /= static_cast<double>(n);

    double numerator = 0.0;
    double denominator = 0.0;
    for (size_t i = 0; i < n; i++) {
        double dt = static_cast<double>(i) - t_mean;
        numerator += dt * (x[i] - x_mean);
        denominator += dt * dt;
    }
    double slope = denominator != 0.0 ? numerator / denominator : 0.0;
    double intercept = x_mean - slope * t_mean;

    std::vector<double> result(n);
    for (size_t i = 0; i < n; i++) {
        result[i] = x[i] - (intercept + slope * static_cast<double>(i));
    }
    return result;
}

struct LocalExtrema {
    std::vector<size_t> peaks;   // 0-based, posição DENTRO da janela interna recebida
    std::vector<size_t> onsets;  // idem
};

// Port de detect_peaks_and_onsets_using_msptd(x) do MATLAB. x_raw é a janela
// interna de 6s (ou o sinal inteiro, se mais curto).
//
// Conversão de índices: no MATLAB, para a escala k (1-based, k=1..L) e
// i=(k+2):(N-k+1), a condição é sobre x(i-1), x(i-k-1), x(i+k-1) (todos
// 1-based), marcando m_max(k,i-1). Substituindo j = i-2 (0-based): j vai de k até
// N-k-1 (0-based), e a condição passa a ser sobre x_0[j], x_0[j-k], x_0[j+k] --
// exatamente como implementado abaixo, sem nenhum deslocamento residual.
LocalExtrema detect_peaks_and_onsets_using_msptd(const std::vector<double>& x_raw) {
    size_t n = x_raw.size();
    LocalExtrema result;

    // L = ceil(N/2) - 1 no MATLAB; ceil(N/2) em inteiros é (N+1)/2.
    if (n < 3) {
        return result;
    }
    size_t l = (n + 1) / 2 - 1;
    if (l < 1) {
        return result;
    }

    std::vector<double> x = detrend_linear(x_raw);

    std::vector<std::vector<bool>> m_max(l, std::vector<bool>(n, false));
    std::vector<std::vector<bool>> m_min(l, std::vector<bool>(n, false));

    for (size_t k = 1; k <= l; k++) {
        for (size_t j = k; j <= n - k - 1; j++) {
            if (x[j] > x[j - k] && x[j] > x[j + k]) {
                m_max[k - 1][j] = true;
            }
            if (x[j] < x[j - k] && x[j] < x[j + k]) {
                m_min[k - 1][j] = true;
            }
        }
    }

    // gamma_max/gamma_min: soma de marcas por escala (linha). lambda = escala com
    // mais marcas, primeira em caso de empate (igual ao max() do MATLAB).
    size_t lambda_max_idx = 0;
    size_t lambda_min_idx = 0;
    size_t best_max_count = 0;
    size_t best_min_count = 0;
    for (size_t k = 0; k < l; k++) {
        size_t max_count = 0;
        size_t min_count = 0;
        for (size_t j = 0; j < n; j++) {
            if (m_max[k][j]) max_count++;
            if (m_min[k][j]) min_count++;
        }
        if (max_count > best_max_count) {
            best_max_count = max_count;
            lambda_max_idx = k;
        }
        if (min_count > best_min_count) {
            best_min_count = min_count;
            lambda_min_idx = k;
        }
    }

    // Pico/início: marcado como máximo/mínimo em TODAS as escalas de 0 até
    // lambda_idx (equivalente a 1..lambda no MATLAB).
    for (size_t j = 0; j < n; j++) {
        bool is_peak = true;
        for (size_t k = 0; k <= lambda_max_idx; k++) {
            if (!m_max[k][j]) {
                is_peak = false;
                break;
            }
        }
        if (is_peak) {
            result.peaks.push_back(j);
        }

        bool is_onset = true;
        for (size_t k = 0; k <= lambda_min_idx; k++) {
            if (!m_min[k][j]) {
                is_onset = false;
                break;
            }
        }
        if (is_onset) {
            result.onsets.push_back(j);
        }
    }

    return result;
}

} // namespace

PPGMsptdResult msptd_beat_detector(const std::vector<double>& sig, int fs) {
    PPGMsptdResult final_result;
    size_t total = sig.size();
    if (total == 0) {
        return final_result;
    }

    // no_samps_in_win é o comprimento NOMINAL em amostras -- no MATLAB,
    // win_starts(i):win_ends(i) é inclusivo nos dois extremos e
    // win_ends = win_starts + no_samps_in_win, então cada janela interna tem na
    // verdade no_samps_in_win+1 amostras.
    constexpr double WIN_LEN_S = 6.0;
    constexpr double OVERLAP_FRACTION = 0.2;
    size_t no_samps_in_win = static_cast<size_t>(std::llround(WIN_LEN_S * static_cast<double>(fs)));

    std::vector<size_t> win_starts; // 0-based, inclusive
    std::vector<size_t> win_ends;   // 0-based, inclusive

    if (total <= no_samps_in_win) {
        // MATLAB: win_starts=1, win_ends=length(sig) -> 0-based: [0, total-1].
        win_starts.push_back(0);
        win_ends.push_back(total - 1);
    } else {
        size_t win_offset = static_cast<size_t>(
            std::llround(static_cast<double>(no_samps_in_win) * (1.0 - OVERLAP_FRACTION)));
        if (win_offset == 0) {
            win_offset = 1; // evita loop infinito num caso degenerado (fs/janela absurdamente pequenos)
        }

        // MATLAB: win_starts = 1:win_offset:(total-no_samps_in_win) (1-based) ->
        // 0-based: 0:win_offset:(total-no_samps_in_win-1).
        size_t last_start_bound = total - no_samps_in_win - 1;
        for (size_t start = 0; start <= last_start_bound; start += win_offset) {
            win_starts.push_back(start);
            win_ends.push_back(start + no_samps_in_win);
        }

        // "esta garante que as janelas cubram a duração inteira do sinal": se a
        // última janela não chegar exatamente ao fim, adiciona uma janela final
        // ancorada no fim do sinal.
        if (win_ends.empty() || win_ends.back() < total - 1) {
            win_starts.push_back(total - no_samps_in_win - 1);
            win_ends.push_back(total - 1);
        }
    }

    // Tolerância do refinamento de posição: ceil(fs*0.05) amostras.
    size_t tol = static_cast<size_t>(std::ceil(static_cast<double>(fs) * 0.05));

    std::vector<size_t> all_peaks;
    std::vector<size_t> all_onsets;

    for (size_t w = 0; w < win_starts.size(); w++) {
        size_t start = win_starts[w];
        size_t end = win_ends[w]; // inclusive

        std::vector<double> win_sig(sig.begin() + static_cast<std::ptrdiff_t>(start),
                                     sig.begin() + static_cast<std::ptrdiff_t>(end) + 1);

        LocalExtrema local = detect_peaks_and_onsets_using_msptd(win_sig);

        // Refinamento: move cada pico pro maior valor (e cada início pro menor
        // valor) do sinal original da janela, dentro de +-tol amostras, limitado
        // às bordas da janela (adaptação própria, ver README, seção 5, "Batimentos").
        for (size_t& p : local.peaks) {
            size_t lo = p >= tol ? p - tol : 0;
            size_t hi = std::min(p + tol, win_sig.size() - 1);
            size_t best = lo;
            for (size_t idx = lo + 1; idx <= hi; idx++) {
                if (win_sig[idx] > win_sig[best]) {
                    best = idx;
                }
            }
            p = best;
        }
        for (size_t& t : local.onsets) {
            size_t lo = t >= tol ? t - tol : 0;
            size_t hi = std::min(t + tol, win_sig.size() - 1);
            size_t best = lo;
            for (size_t idx = lo + 1; idx <= hi; idx++) {
                if (win_sig[idx] < win_sig[best]) {
                    best = idx;
                }
            }
            t = best;
        }

        for (size_t p : local.peaks) {
            all_peaks.push_back(p + start);
        }
        for (size_t t : local.onsets) {
            all_onsets.push_back(t + start);
        }
    }

    // Equivalente ao "tidy up" do MATLAB (ver cabeçalho do módulo).
    std::sort(all_peaks.begin(), all_peaks.end());
    all_peaks.erase(std::unique(all_peaks.begin(), all_peaks.end()), all_peaks.end());
    std::sort(all_onsets.begin(), all_onsets.end());
    all_onsets.erase(std::unique(all_onsets.begin(), all_onsets.end()), all_onsets.end());

    final_result.peaks = std::move(all_peaks);
    final_result.onsets = std::move(all_onsets);
    return final_result;
}
