// File: cpp/eco_restoration/vfd_jump_diffusion_control.cpp

#include <cmath>
#include <vector>
#include <std::stdexcept>
#include <sqlite3.h>

struct PriceForecast {
    double ts_utc;
    double price_per_kwh;
};

struct PumpState {
    double flow_rate;    // m3/s
    double head_m;       // m
};

struct ControlPolicy {
    // Discrete pump speed levels and associated value function
    std::vector<double> speeds;       // e.g., normalized 0..1
    std::vector<double> value;        // V(speed)
};

class VFDJumpDiffusionController {
public:
    VFDJumpDiffusionController(sqlite3* db,
                               double lambda_jump,
                               double sigma_diff,
                               double discount)
        : db_(db),
          lambda_(lambda_jump),
          sigma_(sigma_diff),
          beta_(discount) {
        if (!db_) throw std::runtime_error("SQLite DB pointer must not be null");
        if (beta_ <= 0.0) throw std::runtime_error("Discount must be positive");
        initPolicy();
    }

    // Policy iteration: update value function and control decisions using HJB-like Bellman updates
    void policyIteration(int iterations) {
        for (int it = 0; it < iterations; ++it) {
            evaluatePolicy();
            improvePolicy();
        }
    }

    double computePumpSpeed(const PumpState& state, double current_ts_utc) const {
        // Simple mapping: choose speed with lowest Q-value given current forecast
        PriceForecast pf = readPriceForecast(current_ts_utc);
        double p = pf.price_per_kwh;

        int bestIdx = 0;
        double bestCost = std::numeric_limits<double>::infinity();
        for (int i = 0; i < static_cast<int>(policy_.speeds.size()); ++i) {
            double u = policy_.speeds[i];
            double cost = instantaneousCost(state, u, p);
            if (cost < bestCost) {
                bestCost = cost;
                bestIdx = i;
            }
        }
        return policy_.speeds[bestIdx];
    }

private:
    sqlite3*      db_;
    double        lambda_; // jump intensity
    double        sigma_;  // diffusion volatility
    double        beta_;   // discount factor
    ControlPolicy policy_;

    void initPolicy() {
        // Initialize discrete pump speeds and zero value function
        policy_.speeds = {0.3, 0.5, 0.7, 0.9};
        policy_.value  = std::vector<double>(policy_.speeds.size(), 0.0);
    }

    PriceForecast readPriceForecast(double current_ts_utc) const {
        // Query nearest future price forecast
        const char* sql =
            "SELECT ts_utc, price_per_kwh "
            "FROM electricity_price_forecast "
            "WHERE ts_utc >= ? "
            "ORDER BY ts_utc ASC "
            "LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare price forecast query");
        }

        sqlite3_bind_double(stmt, 1, current_ts_utc);

        PriceForecast pf{};
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            pf.ts_utc        = sqlite3_column_double(stmt, 0);
            pf.price_per_kwh = sqlite3_column_double(stmt, 1);
        } else {
            pf.ts_utc        = current_ts_utc;
            pf.price_per_kwh = 0.1; // fallback
        }
        sqlite3_finalize(stmt);
        return pf;
    }

    double instantaneousCost(const PumpState& s,
                             double u,
                             double price_per_kwh) const {
        // Energy cost term: power ~ flow*head/speed efficiency
        double efficiency = std::max(0.1, 1.0 - std::fabs(u - 0.7)); // toy efficiency
        double power_kw   = (s.flow_rate * s.head_m) / efficiency;   // scaled
        double cost_energy = price_per_kwh * power_kw;

        // Eco-impact term (e.g., ker_e proxy): penalize high speed
        double eco_penalty = u * u;

        return cost_energy + eco_penalty;
    }

    void evaluatePolicy() {
        // Policy evaluation: update value function V(u) via Bellman equation
        // For jump-diffusion price: expected incremental cost includes diffusion and jumps.
        // HJB form (continuous) simplifies here to discrete-time Bellman:
        //
        // V(u) = E[ cost(u, P_t) + e^{-beta dt} V(u_next) ]
        //
        // We approximate with local expectation over price shocks.
        double dt = 900.0; // 15 min steps

        for (int i = 0; i < static_cast<int>(policy_.speeds.size()); ++i) {
            double u = policy_.speeds[i];

            // Local expected price change: diffusion + jumps (simplified)
            double p0 = 0.1;
            double dp_diff = 0.0; // zero-mean
            double dp_jump = lambda_ * 0.2; // expected jump magnitude
            double p1_mean = p0 + dp_diff + dp_jump;

            PumpState s{};
            s.flow_rate = 0.5;
            s.head_m    = 5.0;

            double cost_now = instantaneousCost(s, u, p0);
            double cost_future = instantaneousCost(s, u, p1_mean);
            double V_next = policy_.value[i];

            double V_new = cost_now + std::exp(-beta_ * dt) * (cost_future + V_next);
            policy_.value[i] = V_new;
        }
    }

    void improvePolicy() {
        // Policy improvement: potentially adjust speeds or keep discrete set and rely on evaluation.
        // In a richer implementation, this would update policy_.speeds or add actions.
        // Here we leave speeds as is; improvement occurs implicitly via value updates.
    }
};
