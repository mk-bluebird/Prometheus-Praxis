// Filename: src/ker_cpp/ker_benchmark.cpp

#include "ker_kernel.cpp"
#include <chrono>
#include <random>
#include <vector>

int main() {
    constexpr std::size_t iterations = 1000;
    std::array<double, NUM_PLANES> r{};
    std::array<double, NUM_PLANES> w{};

    std::mt19937_64 rng(123456);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (std::size_t i = 0; i < NUM_PLANES; ++i) {
        w[i] = dist(rng);
    }

    double vtPrev = 0.0;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (std::size_t it = 0; it < iterations; ++it) {
        for (std::size_t i = 0; i < NUM_PLANES; ++i) {
            r[i] = dist(rng);
        }
        KerResult res = computeKer(r, w, vtPrev);
        vtPrev = res.vt;
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    auto dtNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    // On Raspberry Pi, energy measurement is done externally (e.g. via power meter),
    // this program produces consistent compute patterns for correlation.
    return 0;
}
