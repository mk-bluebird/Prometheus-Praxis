// File: cpp/simulation/habitat_regeneration_mpi_monte_carlo.cpp
#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace eco {

struct HabitatPatch {
    std::string hex_id;
    double area_ha;
    double initial_biodiversity_index;
    double restoration_effort_index;
};

struct RegenParams {
    double B_max;
    double base_r;
    double fire_lambda;
    int    years;
};

struct MonteCarloResult {
    std::vector<double> mean_B_per_year;
    std::vector<double> var_B_per_year;
};

double compute_biodiversity_at_time(double B_max,
                                    double B0,
                                    double r_eff,
                                    double disturbance_fraction,
                                    int t_year) {
    double one_minus_d = 1.0 - disturbance_fraction;
    if (one_minus_d < 0.0) one_minus_d = 0.0;
    double exponent = -r_eff * one_minus_d * static_cast<double>(t_year);
    double term = std::exp(exponent);
    return B_max - (B_max - B0) * term;
}

MonteCarloResult run_monte_carlo_for_patch(const HabitatPatch& patch,
                                           const RegenParams& params,
                                           int num_simulations,
                                           unsigned int seed_offset) {
    std::mt19937 rng(seed_offset);
    std::poisson_distribution<int> fire_poisson(params.fire_lambda);

    int years = params.years;
    std::vector<double> sum_B(years + 1, 0.0);
    std::vector<double> sum_B2(years + 1, 0.0);

    double B0   = patch.initial_biodiversity_index;
    double Bmax = params.B_max;
    double r_eff = params.base_r * (0.5 + 0.5 * patch.restoration_effort_index);
    if (r_eff < 0.0) r_eff = 0.0;

    for (int sim = 0; sim < num_simulations; ++sim) {
        double B_t0 = B0;
        sum_B[0]  += B_t0;
        sum_B2[0] += B_t0 * B_t0;

        for (int year = 1; year <= years; ++year) {
            int fires_this_year = fire_poisson(rng);
            double disturbance_fraction =
                std::min(static_cast<double>(fires_this_year) / 3.0, 1.0);
            double B_t = compute_biodiversity_at_time(
                Bmax, B0, r_eff, disturbance_fraction, year
            );
            sum_B[year]  += B_t;
            sum_B2[year] += B_t * B_t;
        }
    }

    MonteCarloResult result;
    result.mean_B_per_year.resize(years + 1);
    result.var_B_per_year.resize(years + 1);

    for (int year = 0; year <= years; ++year) {
        double mean  = sum_B[year] / static_cast<double>(num_simulations);
        double mean2 = sum_B2[year] / static_cast<double>(num_simulations);
        double var   = mean2 - mean * mean;
        if (var < 0.0) var = 0.0;
        result.mean_B_per_year[year] = mean;
        result.var_B_per_year[year]  = var;
    }

    return result;
}

std::string build_geojson_feature(const HabitatPatch& patch,
                                  const MonteCarloResult& mc,
                                  int years) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "{";
    oss << "\"type\":\"Feature\",";
    oss << "\"properties\":{";
    oss << "\"hex_id\":\"" << patch.hex_id << "\",";
    oss << "\"area_ha\":" << patch.area_ha << ",";
    oss << "\"biodiversity_mean\":[";
    for (int year = 0; year <= years; ++year) {
        oss << mc.mean_B_per_year[year];
        if (year < years) oss << ",";
    }
    oss << "],";
    oss << "\"biodiversity_var\":[";
    for (int year = 0; year <= years; ++year) {
        oss << mc.var_B_per_year[year];
        if (year < years) oss << ",";
    }
    oss << "]";
    oss << "},";
    oss << "\"geometry\":{";
    oss << "\"type\":\"Point\",\"coordinates\":[-112.074,33.448]}";
    oss << "}";
    return oss.str();
}

struct EcoScoreResult {
    std::string hex_id;
    double eco_score_mean;
    double eco_score_std;
};

double eco_score_from_biodiversity(double B_norm) {
    double B_clamped = std::max(0.0, std::min(1.0, B_norm));
    return B_clamped;
}

EcoScoreResult aggregate_eco_score(const HabitatPatch& patch,
                                   const MonteCarloResult& mc,
                                   int years) {
    std::vector<double> scores;
    scores.reserve(years + 1);
    for (int year = 0; year <= years; ++year) {
        double B_norm = mc.mean_B_per_year[year];
        scores.push_back(eco_score_from_biodiversity(B_norm));
    }
    double sum = 0.0;
    for (double s : scores) sum += s;
    double mean = sum / static_cast<double>(scores.size());
    double var  = 0.0;
    for (double s : scores) {
        var += (s - mean) * (s - mean);
    }
    var /= static_cast<double>(scores.size());
    double stddev = std::sqrt(var);

    EcoScoreResult r{};
    r.hex_id = patch.hex_id;
    r.eco_score_mean = mean;
    r.eco_score_std  = stddev;
    return r;
}

std::string build_eco_score_sql(const EcoScoreResult& r,
                                const std::string& run_id) {
    std::ostringstream oss;
    oss << "INSERT INTO habitat_regeneration_eco_score "
        << "(run_id, hex_id, eco_score_mean, eco_score_std) VALUES ('"
        << run_id << "', '"
        << r.hex_id << "', "
        << r.eco_score_mean << ", "
        << r.eco_score_std << ");";
    return oss.str();
}

} // namespace eco

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank = 0;
    int world_size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::vector<eco::HabitatPatch> patches;
    patches.push_back(eco::HabitatPatch{"hex_HAB_1", 10.0, 0.40, 0.7});
    patches.push_back(eco::HabitatPatch{"hex_HAB_2", 8.5, 0.35, 0.6});
    patches.push_back(eco::HabitatPatch{"hex_HAB_3", 12.0, 0.45, 0.8});

    eco::RegenParams params;
    params.B_max       = 0.95;
    params.base_r      = 0.10;
    params.fire_lambda = 0.4;
    params.years       = 25;

    int total_simulations = 1000;
    int sims_per_rank     = total_simulations / world_size;
    if (world_rank == world_size - 1) {
        sims_per_rank += total_simulations % world_size;
    }

    int years       = params.years;
    int num_patches = static_cast<int>(patches.size());

    std::vector<eco::MonteCarloResult> local_results;
    local_results.reserve(num_patches);
    for (int p = 0; p < num_patches; ++p) {
        unsigned int seed_offset =
            static_cast<unsigned int>(world_rank * 1000 + p * 13);
        eco::MonteCarloResult mc =
            eco::run_monte_carlo_for_patch(patches[p], params,
                                           sims_per_rank, seed_offset);
        local_results.push_back(mc);
    }

    std::vector<double> local_mean(num_patches * (years + 1), 0.0);
    std::vector<double> local_var(num_patches * (years + 1), 0.0);
    for (int p = 0; p < num_patches; ++p) {
        for (int year = 0; year <= years; ++year) {
            int idx = p * (years + 1) + year;
            local_mean[idx] = local_results[p].mean_B_per_year[year];
            local_var[idx]  = local_results[p].var_B_per_year[year];
        }
    }

    std::vector<double> global_mean(num_patches * (years + 1), 0.0);
    std::vector<double> global_var(num_patches * (years + 1), 0.0);

    MPI_Reduce(local_mean.data(), global_mean.data(),
               num_patches * (years + 1),
               MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce(local_var.data(), global_var.data(),
               num_patches * (years + 1),
               MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        for (int idx = 0; idx < num_patches * (years + 1); ++idx) {
            global_mean[idx] /= static_cast<double>(world_size);
            global_var[idx]  /= static_cast<double>(world_size);
        }

        std::ostringstream geojson;
        geojson << "{";
        geojson << "\"type\":\"FeatureCollection\",\"features\":[";

        std::string run_id = "mpi_monte_carlo_2026_08_03";

        for (int p = 0; p < num_patches; ++p) {
            eco::MonteCarloResult aggregated;
            aggregated.mean_B_per_year.resize(years + 1);
            aggregated.var_B_per_year.resize(years + 1);
            for (int year = 0; year <= years; ++year) {
                int idx = p * (years + 1) + year;
                aggregated.mean_B_per_year[year] = global_mean[idx];
                aggregated.var_B_per_year[year]  = global_var[idx];
            }

            std::string feature =
                eco::build_geojson_feature(patches[p], aggregated, years);
            geojson << feature;
            if (p < num_patches - 1) {
                geojson << ",";
            }

            eco::EcoScoreResult eco_res =
                eco::aggregate_eco_score(patches[p], aggregated, years);
            std::string sql = eco::build_eco_score_sql(eco_res, run_id);
            std::cout << sql << "\n";
        }

        geojson << "]}";
        std::cout << geojson.str() << "\n";
    }

    MPI_Finalize();
    return 0;
}
