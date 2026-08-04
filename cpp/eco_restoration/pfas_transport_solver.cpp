// File: cpp/eco_restoration/pfas_transport_solver.cpp

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

struct OUParams {
    double theta;
    double sigma2;
};

struct PFASState {
    std::vector<double> conc;
    std::vector<double> sorbed;
};

struct GridParams {
    double dx;
    double velocity;
    double dispersion;
    double sorption_coeff_mean;
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
                 std::vector<double>(n_cells, 0.0)},
          k_current_(grid.sorption_coeff_mean) {
        if (!db_) {
            throw std::runtime_error("SQLite DB pointer must not be null");
        }
        if (n_cells_ < 2) {
            throw std::runtime_error("Need at least 2 cells for finite-volume scheme");
        }
        install_schema();
    }

    void updateOUParams(const OUParams& ou) {
        ou_ = ou;
    }

    void readBoundaryConditions(double& inflow_conc, double& flow_rate) {
        const char* sql =
            "SELECT pfas_conc, flow_rate "
            "FROM pfas_telemetry "
            "ORDER BY ts_utc DESC "
            "LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to prepare boundary condition query");
        }
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            inflow_conc = sqlite3_column_double(stmt, 0);
            flow_rate   = sqlite3_column_double(stmt, 1);
        } else {
            inflow_conc = 0.0;
            flow_rate   = 0.0;
        }
        sqlite3_finalize(stmt);
    }

    void step(double dt_seconds) {
        double inflow_conc = 0.0;
        double flow_rate   = 0.0;
        readBoundaryConditions(inflow_conc, flow_rate);

        double u = grid_.velocity;
        if (flow_rate > 0.0) {
            u = flow_rate_to_velocity(flow_rate);
        }

        double k_sorp = updateSorptionCoefficient(dt_seconds);

        std::vector<double> conc_new(n_cells_, 0.0);
        std::vector<double> sorbed_new(n_cells_, 0.0);

        conc_new[0]   = inflow_conc;
        sorbed_new[0] = state_.sorbed[0];

        for (std::size_t i = 1; i < n_cells_ - 1; ++i) {
            double c_i   = state_.conc[i];
            double c_im1 = state_.conc[i - 1];
            double c_ip1 = state_.conc[i + 1];

            double adv  = -u * (c_i - c_im1) / grid_.dx;
            double disp = grid_.dispersion * (c_ip1 - 2.0 * c_i + c_im1) / (grid_.dx * grid_.dx);
            double sorp_rate = k_sorp * c_i;

            double dc_dt = adv + disp - sorp_rate;
            double ds_dt = sorp_rate;

            conc_new[i]   = c_i + dt_seconds * dc_dt;
            sorbed_new[i] = state_.sorbed[i] + dt_seconds * ds_dt;

            if (conc_new[i] < 0.0) conc_new[i] = 0.0;
            if (sorbed_new[i] < 0.0) sorbed_new[i] = 0.0;
        }

        conc_new[n_cells_ - 1]   = conc_new[n_cells_ - 2];
        sorbed_new[n_cells_ - 1] = sorbed_new[n_cells_ - 2];

        state_.conc   = std::move(conc_new);
        state_.sorbed = std::move(sorbed_new);

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
    double k_current_;

    double flow_rate_to_velocity(double flow_rate_m3s) const {
        double area = 1.0;
        return flow_rate_m3s / area;
    }

    double updateSorptionCoefficient(double dt_seconds) {
        double K_mean = grid_.sorption_coeff_mean;
        double theta  = ou_.theta;
        double sigma2 = ou_.sigma2;

        double drift = -theta * (k_current_ - K_mean) * dt_seconds;
        double dW = sampleNormal(0.0, std::sqrt(dt_seconds));
        double diffusion = std::sqrt(sigma2) * dW;

        k_current_ += drift + diffusion;
        if (k_current_ < 0.0) k_current_ = 0.0;
        return k_current_;
    }

    double sampleNormal(double mean, double stddev) {
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

    void install_schema() {
        const char* sql_profile =
            "CREATE TABLE IF NOT EXISTS pfas_profile ("
            " cell_index INTEGER NOT NULL,"
            " ts_utc    INTEGER NOT NULL,"
            " conc      REAL NOT NULL,"
            " sorbed    REAL NOT NULL,"
            " PRIMARY KEY (cell_index, ts_utc)"
            ");";
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql_profile, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "Failed to create pfas_profile: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }
    }

    void writeConcentrationProfile(double dt_seconds) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql_insert =
            "INSERT OR REPLACE INTO pfas_profile (cell_index, ts_utc, conc, sorbed) "
            "VALUES (?, strftime('%s','now'), ?, ?);";
        int rc = sqlite3_prepare_v2(db_, sql_insert, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to prepare pfas_profile insert");
        }
        for (std::size_t i = 0; i < n_cells_; ++i) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(i));
            sqlite3_bind_double(stmt, 2, state_.conc[i]);
            sqlite3_bind_double(stmt, 3, state_.sorbed[i]);
            rc = sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }
};

struct Grid1D {
    double x_min;
    double x_max;
    std::size_t n_cells;
    double dx;
    std::vector<double> x;

    Grid1D(double x_min_, double x_max_, std::size_t n_cells_)
        : x_min(x_min_), x_max(x_max_), n_cells(n_cells_) {
        if (n_cells_ < 2) {
            throw std::invalid_argument("Grid must have at least 2 cells");
        }
        dx = (x_max - x_min) / static_cast<double>(n_cells);
        x.resize(n_cells);
        for (std::size_t i = 0; i < n_cells; ++i) {
            x[i] = x_min + (i + 0.5) * dx;
        }
    }
};

struct PFASTransportParams1D {
    double velocity;
    double dispersion;
    double sorption_base;
    OUParams sorption_ou;
    double dt;
};

struct BoundaryConditions1D {
    double c_inlet;
};

class PFASTransportSolver1D {
public:
    PFASTransportSolver1D(const Grid1D& grid,
                          const PFASTransportParams1D& params,
                          const BoundaryConditions1D& bc,
                          const std::string& db_path)
        : grid_(grid),
          params_(params),
          bc_(bc),
          db_path_(db_path),
          c_(grid.n_cells, 0.0),
          sorption_coeff_(grid.n_cells, params.sorption_base),
          db_(nullptr) {
        open_db();
        install_schema();
    }

    ~PFASTransportSolver1D() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    void initialize_profile() {
        const char* sql =
            "SELECT cell_index, concentration_ugL "
            "FROM pfas_profile_initial "
            "ORDER BY cell_index ASC;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            std::size_t count = 0;
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                int idx = sqlite3_column_int(stmt, 0);
                double c = sqlite3_column_double(stmt, 1);
                if (idx >= 0 && static_cast<std::size_t>(idx) < grid_.n_cells) {
                    c_[static_cast<std::size_t>(idx)] = c;
                    count++;
                }
            }
            sqlite3_finalize(stmt);
            if (count == 0) {
                std::fill(c_.begin(), c_.end(), 0.0);
            }
        } else {
            sqlite3_finalize(stmt);
            std::fill(c_.begin(), c_.end(), 0.0);
        }
    }

    void step(double t_current) {
        std::vector<double> c_new(grid_.n_cells, 0.0);
        for (std::size_t i = 0; i < grid_.n_cells; ++i) {
            double ks = sorption_coeff_[i];
            double theta = params_.sorption_ou.theta;
            double k_base = params_.sorption_base;
            double dt = params_.dt;
            sorption_coeff_[i] = ks + theta * (ks - k_base) * dt;
        }

        double v = params_.velocity;
        double D = params_.dispersion;
        double dt = params_.dt;
        double dx = grid_.dx;

        c_new[0] = bc_.c_inlet;
        for (std::size_t i = 1; i < grid_.n_cells - 1; ++i) {
            double c_i = c_[i];
            double c_im1 = c_[i - 1];
            double c_ip1 = c_[i + 1];
            double adv  = -v * (c_i - c_im1) / dx;
            double disp = D * (c_ip1 - 2.0 * c_i + c_im1) / (dx * dx);
            double sorp = -sorption_coeff_[i] * c_i;
            double rhs = adv + disp + sorp;
            c_new[i] = c_i + dt * rhs;
            if (c_new[i] < 0.0) c_new[i] = 0.0;
        }
        std::size_t N = grid_.n_cells;
        c_new[N - 1] = c_[N - 1];
        c_ = c_new;
        store_profile(t_current + dt);
    }

    const std::vector<double>& concentrations() const {
        return c_;
    }

private:
    Grid1D grid_;
    PFASTransportParams1D params_;
    BoundaryConditions1D bc_;
    std::string db_path_;
    std::vector<double> c_;
    std::vector<double> sorption_coeff_;
    sqlite3* db_;

    void open_db() {
        int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Cannot open DB: " + msg);
        }
    }

    void exec_sql(const char* sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            throw std::runtime_error("SQLite exec error: " + msg);
        }
    }

    void install_schema() {
        const char* sql_profile_initial =
            "CREATE TABLE IF NOT EXISTS pfas_profile_initial ("
            "  cell_index INTEGER PRIMARY KEY,"
            "  concentration_ugL REAL NOT NULL"
            ");";
        exec_sql(sql_profile_initial);
        const char* sql_profile =
            "CREATE TABLE IF NOT EXISTS pfas_profile ("
            "  time_s REAL NOT NULL,"
            "  cell_index INTEGER NOT NULL,"
            "  x_coord_m REAL NOT NULL,"
            "  concentration_ugL REAL NOT NULL,"
            "  PRIMARY KEY(time_s, cell_index)"
            ");";
        exec_sql(sql_profile);
        const char* sql_ou_params =
            "CREATE TABLE IF NOT EXISTS pfas_sorption_ou_params ("
            "  theta REAL NOT NULL,"
            "  sigma2 REAL NOT NULL,"
            "  last_updated TEXT NOT NULL"
            ");";
        exec_sql(sql_ou_params);
    }

    void store_profile(double t_current) {
        exec_sql("BEGIN IMMEDIATE TRANSACTION;");
        const char* sql_del = "DELETE FROM pfas_profile;";
        exec_sql(sql_del);
        const char* sql_ins =
            "INSERT INTO pfas_profile(time_s, cell_index, x_coord_m, concentration_ugL) "
            "VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql_ins, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            exec_sql("ROLLBACK;");
            throw std::runtime_error("Prepare insert pfas_profile failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }
        for (std::size_t i = 0; i < grid_.n_cells; ++i) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_bind_double(stmt, 1, t_current);
            sqlite3_bind_int(stmt, 2, static_cast<int>(i));
            sqlite3_bind_double(stmt, 3, grid_.x[i]);
            sqlite3_bind_double(stmt, 4, c_[i]);
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                exec_sql("ROLLBACK;");
                throw std::runtime_error("Insert pfas_profile step failed: " +
                                         std::string(sqlite3_errmsg(db_)));
            }
        }
        sqlite3_finalize(stmt);
        exec_sql("COMMIT;");
    }
};

class OUParameterFitter {
public:
    OUParameterFitter(const std::string& db_path)
        : db_path_(db_path), db_(nullptr) {
        open_db();
        install_schema();
    }

    ~OUParameterFitter() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    OUParams fit_from_sql() {
        const char* sql =
            "SELECT time_s, k_s FROM pfas_sorption_obs ORDER BY time_s ASC;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare pfas_sorption_obs failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }
        std::vector<double> t;
        std::vector<double> k;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            double time = sqlite3_column_double(stmt, 0);
            double ks = sqlite3_column_double(stmt, 1);
            t.push_back(time);
            k.push_back(ks);
        }
        sqlite3_finalize(stmt);
        if (t.size() < 2) {
            throw std::runtime_error("Not enough sorption observations to fit OU parameters");
        }
        double dt_sum = 0.0;
        for (std::size_t i = 1; i < t.size(); ++i) {
            dt_sum += (t[i] - t[i - 1]);
        }
        double dt = dt_sum / static_cast<double>(t.size() - 1);
        double sum_xx = 0.0, sum_xy = 0.0;
        for (std::size_t i = 0; i + 1 < k.size(); ++i) {
            double x = k[i];
            double y = k[i + 1];
            sum_xx += x * x;
            sum_xy += x * y;
        }
        double phi = (sum_xx > 0.0) ? sum_xy / sum_xx : 1.0;
        double sum_res2 = 0.0;
        std::size_t count = 0;
        for (std::size_t i = 0; i + 1 < k.size(); ++i) {
            double x = k[i];
            double y = k[i + 1];
            double y_hat = phi * x;
            double res = y - y_hat;
            sum_res2 += res * res;
            count++;
        }
        double q = (count > 0) ? sum_res2 / static_cast<double>(count) : 0.0;
        double theta = (phi > 0.0) ? std::log(phi) / dt : 0.0;
        double sigma2 = (std::abs(theta) > 1e-6)
            ? q * 2.0 * theta / (std::exp(2.0 * theta * dt) - 1.0)
            : q / dt;
        OUParams params{theta, sigma2};
        store_params(params);
        return params;
    }

private:
    std::string db_path_;
    sqlite3* db_;

    void open_db() {
        int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Cannot open DB: " + msg);
        }
    }

    void install_schema() {
        const char* sql_ou_params =
            "CREATE TABLE IF NOT EXISTS pfas_sorption_ou_params ("
            "  theta REAL NOT NULL,"
            "  sigma2 REAL NOT NULL,"
            "  last_updated TEXT NOT NULL"
            ");";
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql_ou_params, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            throw std::runtime_error("Failed to create pfas_sorption_ou_params: " + msg);
        }
        const char* sql_obs =
            "CREATE TABLE IF NOT EXISTS pfas_sorption_obs ("
            "  time_s REAL NOT NULL,"
            "  k_s REAL NOT NULL"
            ");";
        rc = sqlite3_exec(db_, sql_obs, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            throw std::runtime_error("Failed to create pfas_sorption_obs: " + msg);
        }
    }

    void store_params(const OUParams& params) {
        const char* sql =
            "INSERT INTO pfas_sorption_ou_params(theta, sigma2, last_updated) "
            "VALUES(?, ?, datetime('now'));";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare insert pfas_sorption_ou_params failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }
        sqlite3_bind_double(stmt, 1, params.theta);
        sqlite3_bind_double(stmt, 2, params.sigma2);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            throw std::runtime_error("Insert pfas_sorption_ou_params step failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char** argv) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "pfas_transport.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    try {
        OUParameterFitter fitter(db_path);
        OUParams ou = fitter.fit_from_sql();
        std::cout << "Fitted OU sorption params: theta=" << ou.theta
                  << " sigma2=" << ou.sigma2 << std::endl;

        sqlite3* db = nullptr;
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            std::cerr << "Failed to open SQLite DB\n";
            return 1;
        }

        GridParams grid;
        grid.dx                  = 10.0;
        grid.velocity            = 0.05;
        grid.dispersion          = 0.01;
        grid.sorption_coeff_mean = 0.001;

        PFASTransportSolver solver(db, 100, grid, ou);

        const double dt = 60.0;
        for (int step = 0; step < 1000; ++step) {
            solver.step(dt);
        }

        sqlite3_close(db);

        Grid1D grid1d(0.0, 1000.0, 100);
        PFASTransportParams1D params1d;
        params1d.velocity      = 0.01;
        params1d.dispersion    = 0.1;
        params1d.sorption_base = 0.001;
        params1d.sorption_ou   = ou;
        params1d.dt            = 10.0;

        BoundaryConditions1D bc;
        bc.c_inlet = 0.5;

        PFASTransportSolver1D solver1d(grid1d, params1d, bc, db_path);
        solver1d.initialize_profile();

        double t = 0.0;
        for (int step = 0; step < 100; ++step) {
            solver1d.step(t);
            t += params1d.dt;
        }

        std::cout << "PFAS transport simulations completed." << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "PFAS transport solver error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
