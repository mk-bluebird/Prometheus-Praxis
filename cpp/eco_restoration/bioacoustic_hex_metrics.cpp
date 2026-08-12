// File: cpp/eco_restoration/bioacoustic_hex_metrics.cpp
#include <sndfile.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

void fft(std::vector<std::complex<double>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> wlen = std::polar(1.0, -2.0 * std::acos(-1.0) / len);
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w{1.0};
            for (std::size_t j = 0; j < len / 2; ++j) {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wlen;
            }
        }
    }
}

struct BioacousticMetric {
    double acoustic_complexity_index{};
    double biodiversity_score{};
    double knowledge_factor{};
    double eco_impact_value{};
};

BioacousticMetric acoustic_complexity(const std::string& audio_path, double aci_reference) {
    if (aci_reference <= 0.0) throw std::invalid_argument("ACI reference must be positive");
    SF_INFO info{};
    std::unique_ptr<SNDFILE, decltype(&sf_close)> audio(sf_open(audio_path.c_str(), SFM_READ, &info), sf_close);
    if (!audio || info.channels < 1) throw std::runtime_error("cannot read audio recording");

    constexpr std::size_t window = 2048;
    std::vector<double> samples(window * static_cast<std::size_t>(info.channels));
    std::vector<double> previous(window / 2), variation(window / 2), amplitude(window / 2);
    std::size_t frames = 0;

    while (sf_readf_double(audio.get(), samples.data(), window) == static_cast<sf_count_t>(window)) {
        std::vector<std::complex<double>> spectrum(window);
        for (std::size_t i = 0; i < window; ++i) {
            double mono = 0.0;
            for (int channel = 0; channel < info.channels; ++channel)
                mono += samples[i * info.channels + channel];
            spectrum[i] = {mono / info.channels *
                (0.5 - 0.5 * std::cos(2.0 * std::acos(-1.0) * i / (window - 1))), 0.0};
        }
        fft(spectrum);
        for (std::size_t bin = 1; bin < window / 2; ++bin) {
            const double current = std::abs(spectrum[bin]);
            if (frames > 0) variation[bin] += std::abs(current - previous[bin]);
            amplitude[bin] += current;
            previous[bin] = current;
        }
        ++frames;
    }

    double numerator = 0.0, denominator = 0.0;
    for (std::size_t bin = 1; bin < window / 2; ++bin) {
        numerator += variation[bin]; denominator += amplitude[bin];
    }
    const double aci = denominator > 0.0 ? numerator / denominator : 0.0;
    const double biodiversity = std::clamp(aci / aci_reference, 0.0, 1.0);
    const double knowledge = std::clamp(static_cast<double>(frames) / 120.0, 0.0, 1.0);
    return {aci, biodiversity, knowledge, biodiversity * knowledge};
}

void persist_bioacoustic_metric(sqlite3* database, std::uint64_t hex_anchor,
                                std::int64_t observed_unix_s, const BioacousticMetric& metric) {
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS hex_bioacoustic_metric("
        "hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
        "acoustic_complexity_index REAL NOT NULL,biodiversity_score REAL NOT NULL "
        "CHECK(biodiversity_score BETWEEN 0 AND 1),knowledge_factor REAL NOT NULL "
        "CHECK(knowledge_factor BETWEEN 0 AND 1),eco_impact_value REAL NOT NULL "
        "CHECK(eco_impact_value BETWEEN 0 AND 1),PRIMARY KEY(hex_anchor,observed_unix_s)) STRICT;",
        nullptr, nullptr, nullptr);
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_bioacoustic_metric VALUES(?,?,?,?,?,?) "
        "ON CONFLICT(hex_anchor,observed_unix_s) DO UPDATE SET "
        "acoustic_complexity_index=excluded.acoustic_complexity_index,"
        "biodiversity_score=excluded.biodiversity_score,"
        "knowledge_factor=excluded.knowledge_factor,eco_impact_value=excluded.eco_impact_value;",
        -1, &statement, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> close_statement(statement, sqlite3_finalize);
    sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(hex_anchor));
    sqlite3_bind_int64(statement, 2, observed_unix_s);
    sqlite3_bind_double(statement, 3, metric.acoustic_complexity_index);
    sqlite3_bind_double(statement, 4, metric.biodiversity_score);
    sqlite3_bind_double(statement, 5, metric.knowledge_factor);
    sqlite3_bind_double(statement, 6, metric.eco_impact_value);
    if (sqlite3_step(statement) != SQLITE_DONE) throw std::runtime_error("bioacoustic upsert failed");
}

}  // namespace eco_restoration
