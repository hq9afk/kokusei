// Radix-2 DIT FFT adapted from GLava by jarcode-foss, licensed under GPL-3.0.

#include <algorithm>
#include <cmath>

#include "modules/resonance/fft.h"

namespace {

constexpr double kTwoPi = 6.28318530718;

double hann(double t, double sz) {
    return 0.53836 - (0.46164 * std::cos(kTwoPi * t / sz));
}

} // namespace

void resonance_fft(float *samples, int n_samples, float fft_scale,
                   float fft_cutoff) {
    float *data = samples;
    unsigned long nn = static_cast<unsigned long>(n_samples) / 2;

    unsigned long n, mmax, m, j, istep, i;
    float wtemp, wr, wpr, wpi, wi, theta;
    float tempr, tempi;

    for (i = 0; i < static_cast<unsigned long>(n_samples); ++i)
        data[i] *= static_cast<float>(
            hann(static_cast<double>(i), static_cast<double>(n_samples - 1)));

    n = nn << 1;
    j = 1;
    for (i = 1; i < n; i += 2) {
        if (j > i) {
            std::swap(data[j - 1], data[i - 1]);
            std::swap(data[j], data[i]);
        }
        m = nn;
        while (m >= 2 && j > m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    mmax = 2;
    while (n > mmax) {
        istep = mmax << 1;
        theta = static_cast<float>(-(2 * M_PI / static_cast<double>(mmax)));
        wtemp = std::sin(0.5f * theta);
        wpr = -2.0f * wtemp * wtemp;
        wpi = std::sin(theta);
        wr = 1.0f;
        wi = 0.0f;
        for (m = 1; m < mmax; m += 2) {
            for (i = m; i <= n; i += istep) {
                j = i + mmax;
                tempr = wr * data[j - 1] - wi * data[j];
                tempi = wr * data[j] + wi * data[j - 1];

                data[j - 1] = data[i - 1] - tempr;
                data[j] = data[i] - tempi;
                data[i - 1] += tempr;
                data[i] += tempi;
            }
            wtemp = wr;
            wr += wr * wpr - wi * wpi;
            wi += wi * wpr + wtemp * wpi;
        }
        mmax = istep;
    }

    for (n = 0; n < static_cast<unsigned long>(n_samples); n += 2) {
        if (data[n] < 0.0f)
            data[n] = -data[n];

        data[n] = std::sqrt(data[n] * data[n] + data[n + 1] * data[n + 1]);
        data[n] = std::log(data[n] + 1.0f) / 3.0f;
        data[n] *= std::max((static_cast<float>(n) /
                             static_cast<float>(n_samples) * fft_scale) +
                                (1.0f - fft_cutoff),
                            1.0f);

        data[n + 1] = data[n];
    }
}
