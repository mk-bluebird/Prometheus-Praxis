// File: cpp/tools/eco_profiler.cpp
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace eco_profiler {

struct RunProfile {
    std::string label;
    double cpu_time_sec;
    long max_rss_kb;
};

class Profiler {
public:
    explicit Profiler(const std::string& label)
        : label_(label),
          start_wall_(std::chrono::steady_clock::now()),
          start_cpu_(std::clock()) {}

    RunProfile finish() {
        auto end_wall = std::chrono::steady_clock::now();
        std::clock_t end_cpu = std::clock();

        double cpu_sec = double(end_cpu - start_cpu_) / double(CLOCKS_PER_SEC);

        long rss_kb = -1;
#if defined(__linux__) || defined(__APPLE__)
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
            rss_kb = usage.ru_maxrss; // already in kilobytes
#else
            rss_kb = usage.ru_maxrss; // kilobytes on Linux
#endif
        }
#endif

        RunProfile profile{};
        profile.label = label_;
        profile.cpu_time_sec = cpu_sec;
        profile.max_rss_kb = rss_kb;
        return profile;
    }

private:
    std::string label_;
    std::chrono::steady_clock::time_point start_wall_;
    std::clock_t start_cpu_;
};

void log_profile(const RunProfile& p, const std::string& path) {
    std::ofstream out(path, std::ios::app);
    if (!out) {
        throw std::runtime_error("Failed to open profiler log: " + path);
    }
    out << p.label << ","
        << p.cpu_time_sec << ","
        << p.max_rss_kb << "\n";
}

} // namespace eco_profiler

// Example usage: wrap a simulation run.
int main() {
    eco_profiler::Profiler prof("multiplane_risk_harness_run");

    // Simulated workload; in practice, call actual simulation functions here.
    volatile double acc = 0.0;
    for (int i = 0; i < 1000000; ++i) {
        acc += std::sin(double(i) * 1e-6);
    }

    eco_profiler::RunProfile p = eco_profiler::profiler::RunProfile{};
    p = prof.finish();

    eco_profiler::log_profile(p, "eco_profiler.log");

    std::cout << "Profile: label=" << p.label
              << " cpu_time_sec=" << p.cpu_time_sec
              << " max_rss_kb=" << p.max_rss_kb << "\n";

    return 0;
}
