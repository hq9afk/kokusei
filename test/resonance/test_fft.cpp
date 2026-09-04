#include <cassert>
#include <cmath>
#include <numbers>
#include <vector>

#include "config/resonance_config.h"

#include "modules/resonance/fft.h"

namespace {

int peak_bin(const std::vector<float> &data, int complex_points) {
    int best = 1;
    float best_v = -1.0f;
    for (int k = 1; k < complex_points / 2; ++k) {
        float v = data[static_cast<size_t>(2 * k)];
        if (v > best_v) {
            best_v = v;
            best = k;
        }
    }
    return best;
}

} // namespace

void test_resonance_fft() {
    const int n = kResonanceFragmentSize;
    const int complex_points = n / 2;

    std::vector<float> silence(static_cast<size_t>(n), 0.0f);
    resonance_fft(silence.data(), n, kResonanceFftScale, kResonanceFftCutOff);
    for (float v : silence)
        assert(v == 0.0f);

    const int tone_bin = 64;
    std::vector<float> tone(static_cast<size_t>(n), 0.0f);
    for (int i = 0; i < complex_points; ++i)
        tone[static_cast<size_t>(2 * i)] = std::cos(
            2.0f * std::numbers::pi_v<float> * static_cast<float>(tone_bin) *
            static_cast<float>(i) / static_cast<float>(complex_points));

    resonance_fft(tone.data(), n, kResonanceFftScale, kResonanceFftCutOff);

    int p = peak_bin(tone, complex_points);
    assert(std::abs(p - tone_bin) <= 2);

    float at_tone = tone[static_cast<size_t>(2 * tone_bin)];
    float far = tone[static_cast<size_t>(2 * (complex_points / 2 - 4))];
    assert(at_tone > far * 4.0f);
}
