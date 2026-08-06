// File: cpp/simulation/cyboquatic_workload_2026_08_04.cpp

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>
#include <map>

/**
 * Cyboquatic workload simulator for eco-restoration machinery.
 *
 * Models:
 *  - energyreqJ: energy requirement in Joules per operation
 *  - deltaVt: effective velocity-time workload metric
 *
 * Design goals:
 *  - Carbon-aware: encourages low-energy schedules and profiles
 *  - Telemetry-ready: prints structured, line-based logs for SQLite ingestion
 */

namespace cyboquatic {

struct WorkloadProfile {
    std::string machine_id;
    std::string location_id;
    double energyreqJ_mean;
    double energyreqJ_std;
    double deltaVt_mean;
    double deltaVt_std;
    double eco_score_bias; // higher bias pushes profiles toward eco-positive behavior
};

struct WorkloadSample {
    std::string timestamp_iso8601;
    std::string machine_id;
    std::string location_id;
    double energyreqJ;
    double deltaVt;
    double eco_intensity; // 0..1, lower is better (less energy per unit eco-work)
    double eco_score;     // composite eco-score
};

class WorkloadSimulator {
public:
    explicit WorkloadSimulator(unsigned int seed = std::random_device{}())
        : rng(seed),
          normal01(0.0, 1.0)
    {}

    void add_profile(const WorkloadProfile& profile) {
        profiles.push_back(profile);
    }

    std::vector<WorkloadSample> simulate_profile(
        const WorkloadProfile& p,
        std::size_t n_samples,
        double eco_weight_energy,
        double eco_weight_deltaVt
    ) {
        std::vector<WorkloadSample> out;
        out.reserve(n_samples);

        for (std::size_t i = 0; i < n_samples; ++i) {
            double e = sample_gaussian_positive(p.energyreqJ_mean, p.energyreqJ_std);
            double dv = sample_gaussian_positive(p.deltaVt_mean, p.deltaVt_std);

            double eco_intensity = compute_eco_intensity(e, dv);
            double eco_score = compute_eco_score(eco_intensity,
                                                 eco_weight_energy,
                                                 eco_weight_deltaVt,
                                                 p.eco_score_bias);

            WorkloadSample s;
            s.timestamp_iso8601 = current_time_iso8601();
            s.machine_id = p.machine_id;
            s.location_id = p.location_id;
            s.energyreqJ = e;
            s.deltaVt = dv;
            s.eco_intensity = eco_intensity;
            s.eco_score = eco_score;

            out.push_back(s);
        }

        return out;
    }

    std::vector<WorkloadSample> simulate_all(
        std::size_t n_samples_each,
        double eco_weight_energy,
        double eco_weight_deltaVt
    ) {
        std::vector<WorkloadSample> out;
        for (const auto& p : profiles) {
            auto partial = simulate_profile(p, n_samples_each,
                                            eco_weight_energy,
                                            eco_weight_deltaVt);
            out.insert(out.end(), partial.begin(), partial.end());
        }
        return out;
    }

    static void print_as_csv(const std::vector<WorkloadSample>& samples, std::ostream& os) {
        os << "timestamp_iso8601,machine_id,location_id,energyreqJ,deltaVt,eco_intensity,eco_score\n";
        os << std::fixed << std::setprecision(6);
        for (const auto& s : samples) {
            os << s.timestamp_iso8601 << ','
               << s.machine_id << ','
               << s.location_id << ','
               << s.energyreqJ << ','
               << s.deltaVt << ','
               << s.eco_intensity << ','
               << s.eco_score << '\n';
        }
    }

private:
    std::mt19937 rng;
    std::normal_distribution<double> normal01;

    double sample_gaussian_positive(double mean, double stddev) {
        if (stddev <= 0.0) {
            return std::max(mean, 0.0);
        }
        double val;
        int guard = 0;
        do {
            double z = normal01(rng);
            val = mean + stddev * z;
            ++guard;
        } while (val <= 0.0 && guard < 16);
        if (val <= 0.0) {
            val = std::max(mean * 0.5, 1e-6);
        }
        return val;
    }

    static double compute_eco_intensity(double energyreqJ, double deltaVt) {
        if (deltaVt <= 0.0) {
            return 1.0; // worst case
        }
        double base = energyreqJ / deltaVt;
        double scaled = base / 1000.0;
        if (scaled > 1.0) scaled = 1.0;
        if (scaled < 0.0) scaled = 0.0;
        return scaled;
    }

    static double compute_eco_score(double eco_intensity,
                                    double eco_weight_energy,
                                    double eco_weight_deltaVt,
                                    double eco_bias) {
        double w_sum = eco_weight_energy + eco_weight_deltaVt;
        if (w_sum <= 0.0) {
            w_sum = 1.0;
            eco_weight_energy = 0.5;
            eco_weight_deltaVt = 0.5;
        }
        double w_energy = eco_weight_energy / w_sum;
        double w_delta = eco_weight_deltaVt / w_sum;
        double raw = (1.0 - eco_intensity) * w_energy + (1.0 - eco_intensity) * w_delta;
        double score = raw + eco_bias;
        if (score > 1.0) score = 1.0;
        if (score < 0.0) score = 0.0;
        return score;
    }

    static std::string current_time_iso8601() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::tm buf{};
#if defined(_WIN32) || defined(_WIN64)
        gmtime_s(&buf, &t);
#else
        gmtime_r(&t, &buf);
#endif
        std::ostringstream oss;
        oss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%S");
        oss << '.' << std::setw(3) << std::setfill('0') << ms.count() << "Z";
        return oss.str();
    }
};

} // namespace cyboquatic

int main() {
    using namespace cyboquatic;

    WorkloadSimulator sim;

    WorkloadProfile sediment_cleaner{
        "machine_sediment_cleaner_v1",
        "canal_sector_alpha",
        5000.0,  500.0,
        200.0,   20.0,
        0.1
    };

    WorkloadProfile wetland_aerator{
        "machine_wetland_aerator_v2",
        "wetland_beta",
        8000.0,  800.0,
        300.0,   30.0,
        0.05
    };

    WorkloadProfile pfos_filter_unit{
        "machine_pfas_filter_v1",
        "treatment_gamma",
        12000.0, 1000.0,
        250.0,   25.0,
        0.08
    };

    sim.add_profile(sediment_cleaner);
    sim.add_profile(wetland_aerator);
    sim.add_profile(pfos_filter_unit);

    double eco_weight_energy = 0.7;
    double eco_weight_deltaVt = 0.3;
    std::size_t n_samples_each = 50;

    auto samples = sim.simulate_all(n_samples_each,
                                    eco_weight_energy,
                                    eco_weight_deltaVt);

    WorkloadSimulator::print_as_csv(samples, std::cout);

    return 0;
}
