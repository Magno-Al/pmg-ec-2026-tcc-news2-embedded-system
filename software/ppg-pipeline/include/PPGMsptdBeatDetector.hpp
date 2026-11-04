#ifndef PPG_MSPTD_BEAT_DETECTOR_HPP
#define PPG_MSPTD_BEAT_DETECTOR_HPP

// Tradução para C++ de source/msptd_beat_detector.m, do repositório ppg-beats de
// Peter H. Charlton: https://github.com/peterhcharlton/ppg-beats (também
// arquivado no Zenodo, DOI 10.5281/zenodo.6037646). NÃO é o arquivo original, é
// uma tradução deste projeto para C++17, mantendo o aviso de licença abaixo por
// se tratar de uma obra derivada.
//
// Algoritmo: MSPTD (Multi-Scale Peak and Trough Detection), de
// S. M. Bishop e A. Ercole, "Multi-scale peak and trough detection optimised
// for periodic and quasi-periodic neuroscience data," in Intracranial Pressure
// and Neuromonitoring XVI, Acta Neurochirurgica Supplement, T. Heldt, Ed.
// Springer, 2018, vol. 126, pp. 189-195. https://doi.org/10.1007/978-3-319-65798-1_39
//
// Selecionado a partir do estudo comparativo: Charlton et al., "Detecting beats
// in the photoplethysmogram: benchmarking open-source algorithms," Physiological
// Measurement, 43(8):085007, 2022.
//
// License - MIT
//    Copyright (c) 2022 Peter H. Charlton
//    Permission is hereby granted, free of charge, to any person obtaining a
//    copy of this software and associated documentation files (the
//    "Software"), to deal in the Software without restriction, including
//    without limitation the rights to use, copy, modify, merge, publish,
//    distribute, sublicense, and/or sell copies of the Software, and to permit
//    persons to whom the Software is furnished to do so, subject to the
//    following conditions:
//    The above copyright notice and this permission notice shall be included
//    in all copies or substantial portions of the Software.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//    DEALINGS IN THE SOFTWARE.
//
// NÃO inclui tidy_beats.m (GPL-3.0, incompatível com a licença deste projeto) —
// no original ele só transforma o vetor de índices em coluna e remove
// duplicatas; aqui o mesmo efeito é obtido com std::sort + std::unique da
// biblioteca padrão (ver PPGMsptdBeatDetector.cpp), sem depender do código GPL.
//
// Downsampling (do_ds no original) está desligado no original e NÃO foi
// portado, conforme pedido.

#include <cstddef>
#include <vector>

struct PPGMsptdResult {
    std::vector<size_t> peaks;   // posições (índice de amostra, 0-based) dos picos
    std::vector<size_t> onsets;  // posições (0-based) dos inícios de pulso (vales)
};

// sig: um canal do sinal (ex.: já filtrado) de uma janela; fs: taxa de amostragem
// em Hz. Detecta picos e inícios de pulso pelo MSPTD, sobre sub-janelas internas de
// 6s com 20% de sobreposição (ver implementação para a conversão de índices
// 1-based/fim-inclusivo do MATLAB original para 0-based).
PPGMsptdResult msptd_beat_detector(const std::vector<double>& sig, int fs);

#endif
