// File: cpp/eco_restoration/pfas_transport_solver.cpp

#include <cmath>
#include <vector>
#include <stdexcept>
#include <string>
#include <iostream>
#include <sqlite3.h>

struct OUParams {
    double theta; // mean-reversion rate
    double sigma; // noise intensity
};

struct PFASState {
    std::vector<double> conc;     // dissolved concentration [mass/volume]
    std::vector<double> sorbed;   // sorbed fraction [mass/volume]
};

struct GridParams {
    double dx;                    // spatial step [m]
    double velocity;              // advection velocity [m/s]
    double dispersion;            // dispersion coefficient [m^2/s]
    double sorption_coeff_mean;   // mean sorption coefficient
};

class PFASTransportSolver {
public:
    PFASTransportSolver(sqlite3* db,
                        std::size_t n_cells,
                        const GridParams& grid,
                        const OUParams& ou)
        : db_(db),
          n_cells_(n_cells),
          grid_(grid),
          ou_(ou),
          state_{std::vector<double>(n_cells, 0.0),
                 std::vector<double>(n_cells, 0.0)}
    {
        if (!db_) {
            throw std::runtime_error("SQLite DB pointer must not be null");
        }
        if (n_cells_ < 2) {
            throw std::runtime_error("Need at least 2 cells for finite-volume scheme");
        }
    }

    void updateOUParams(const OUParams& ou) {
        ou_ = ou;
    }

    // Reads latest PFAS concentration and flow from SQLite (boundary condition)
    void readBoundaryConditions(double& inflow_conc, double& flow_rate) {
        const char* sql =
            "SELECT pfas_conc, flow_rate "
            "FROM pfas_telemetry "
            "ORDER BY ts_utc DESC "
            "LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare boundary condition query");
        }

        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            inflow_conc = sqlite3_column_double(stmt, 0);
            flow_rate   = sqlite3_column_double(stmt, 1);
        } else {
            inflow_conc = 0.0;
            flow_rate   = 0.0;
        }

        sqlite3_finalize(stmt);
    }

    // One time step of 1D advection-dispersion-sorption with OU noise on sorption coeff.
    void step(double dt_seconds) {
        // Read boundary conditions
        double inflow_conc = 0.0;
        double flow_rate   = 0.0;
        readBoundaryConditions(inflow_conc, flow_rate);

        // Effective velocity can be adjusted by flow_rate if desired
        double u = grid_.velocity;
        if (flow_rate > 0.0) {
            u = flow_rate_to_velocity(flow_rate);
        }

        // Compute OU-updated sorption coefficient
        double k_sorp = updateSorptionCoefficient(dt_seconds);

        // Finite-volume update arrays
        std::vector<double> conc_new(n_cells_, 0.0);
        std::vector<double> sorbed_new(n_cells_, 0.0);

        // Boundary cell (upstream): Dirichlet inflow concentration
        conc_new[0]   = inflow_conc;
        sorbed_new[0] = state_.sorbed[0];

        // Internal cells: advection-dispersion-sorption with explicit scheme
        for (std::size_t i = 1; i < n_cells_ - 1; ++i) {
            double c_i   = state_.conc[i];
            double c_im1 = state_.conc[i - 1];
            double c_ip1 = state_.conc[i + 1];

            // Advection term (upwind)
            double adv = -u * (c_i - c_im1) / grid_.dx;

            // Dispersion term (central)
            double disp = grid_.dispersion * (c_ip1 - 2.0 * c_i + c_im1) / (grid_.dx * grid_.dx);

            // Sorption term: simple first-order sink into sorbed pool
            double sorp_rate = k_sorp * c_i;

            double dc_dt = adv + disp - sorp_rate;
            double ds_dt = sorp_rate;

            conc_new[i]   = c_i + dt_seconds * dc_dt;
            sorbed_new[i] = state_.sorbed[i] + dt_seconds * ds_dt;

            if (conc_new[i] < 0.0) conc_new[i] = 0.0;
            if (sorbed_new[i] < 0.0) sorbed_new[i] = 0.0;
        }

        // Downstream boundary: zero-gradient (Neumann) condition
        conc_new[n_cells_ - 1]   = conc_new[n_cells_ - 2];
        sorbed_new[n_cells_ - 1] = sorbed_new[n_cells_ - 2];

        state_.conc   = std::move(conc_new);
        state_.sorbed = std::move(sorbed_new);

        // Write updated concentration profile back to SQLite
        writeConcentrationProfile(dt_seconds);
    }

    const PFASState& state() const {
        return state_;
    }

private:
    sqlite3* db_;
    std::size_t n_cells_;
    GridParams grid_;
    OUParams ou_;
    PFASState state_;

    double flow_rate_to_velocity(double flow_rate_m3s) const {
        // Simple mapping; in real systems use cross-section area.
        double area = 1.0; // m^2, placeholder for canal cross-section
        return flow_rate_m3s / area;
    }

    // OU update for sorption coefficient: dK = -theta(K - K_mean) dt + sigma dW_t
    double updateSorptionCoefficient(double dt_seconds) {
        static double k_current = grid_.sorption_coeff_mean;

        double K_mean = grid_.sorption_coeff_mean;
        double theta  = ou_.theta;
        double sigma  = ou_.sigma;

        // Deterministic part
        double drift = -theta * (k_current - K_mean) * dt_seconds;

        // Stochastic part: approximate Brownian increment via normal(0, sqrt(dt))
        double dW = sampleNormal(0.0, std::sqrt(dt_seconds));
        double diffusion = sigma * dW;

        k_current += drift + diffusion;

        if (k_current < 0.0) k_current = 0.0;
        return k_current;
    }

    double sampleNormal(double mean, double stddev) {
        // Simple Box-Muller for demonstration; in production use a robust RNG.
        static bool hasSpare = false;
        static double spare  = 0.0;

        if (hasSpare) {
            hasSpare = false;
            return mean + stddev * spare;
        }

        double u, v, s;
        do {
            u = 2.0 * randUniform() - 1.0;
            v = 2.0 * randUniform() - 1.0;
            s = u * u + v * v;
        } while (s >= 1.0 || s == 0.0);

        double mul = std::sqrt(-2.0 * std::log(s) / s);
        spare      = v * mul;
        hasSpare   = true;
        return mean + stddev * (u * mul);
    }

    double randUniform() {
        return static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
    }

    void writeConcentrationProfile(double dt_seconds) {
        const char* sql_create =
            "CREATE TABLE IF NOT EXISTS pfas_profile ("
            " cell_index INTEGER NOT NULL,"
            " ts_utc    INTEGER NOT NULL,"
            " conc      REAL NOT NULL,"
            " sorbed    REAL NOT NULL,"
            " PRIMARY KEY (cell_index, ts_utc)"
            ");";

        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql_create, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "Failed to create pfas_profile: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }

        // Use current wall-clock as surrogate timestep timestamp
        sqlite3_stmt* stmt = nullptr;
        const char* sql_insert =
            "INSERT OR REPLACE INTO pfas_profile (cell_index, ts_utc, conc, sorbed) "
            "VALUES (?, strftime('%s','now'), ?, ?);";

        if (sqlite3_prepare_v2(db_, sql_insert, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare pfas_profile insert");
        }

        for (std::size_t i = 0; i < n_cells_; ++i) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(i));
            sqlite3_bind_double(stmt, 2, state_.conc[i]);
            sqlite3_bind_double(stmt, 3, state_.sorbed[i]);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                sqlite3_reset(stmt);
                continue;
            }
            sqlite3_reset(stmt);
        }

        sqlite3_finalize(stmt);
    }
};

// Example of embedding in an online loop
int main(int argc, char** argv) {
    sqlite3* db = nullptr;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
        std::cerr << "Failed to open in-memory SQLite\n";
        return 1;
    }

    GridParams grid;
    grid.dx                  = 10.0;
    grid.velocity            = 0.05;
    grid.dispersion          = 0.01;
    grid.sorption_coeff_mean = 0.001;

    OUParams ou;
    ou.theta = 0.05;
    ou.sigma = 0.0001;

    PFASTransportSolver solver(db, 100, grid, ou);

    const double dt = 60.0; // 60 s timestep
    for (int step = 0; step < 1000; ++step) {
        solver.step(dt);
    }

    sqlite3_close(db);
    return 0;
}
