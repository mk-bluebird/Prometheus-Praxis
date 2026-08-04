// File: cpp/eco_restoration/vfd_jump_diffusion_controller.cpp

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

struct PriceJumpDiffusionParams {
    double mu;
    double sigma;
    double lambda;
    double jump_mean;
    double jump_std;
};

struct PumpConfig {
    double speed_min;
    double speed_max;
    int    speed_levels;
    double dt_hours;
};

struct VFDState {
    double reservoir_level;
};

class VFDJumpDiffusionController {
public:
    VFDJumpDiffusionController(const std::string& db_path,
                               const PumpConfig& pump_cfg,
                               const PriceJumpDiffusionParams& price_params)
        : db_path_(db_path),
          db_(nullptr),
          pump_cfg_(pump_cfg),
          price_params_(price_params),
          n_actions_(pump_cfg.speed_levels),
          gamma_(0.99) {
        open_db();
        install_schema();
        build_action_set();
        init_value_function();
    }

    ~VFDJumpDiffusionController() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    void policy_iteration(int iterations) {
        for (int it = 0; it < iterations; ++it) {
            policy_evaluation();
            policy_improvement();
        }
        store_value_function();
    }

    double select_pump_speed(const VFDState& state, double current_price) const {
        int best_idx = 0;
        double best_q = std::numeric_limits<double>::infinity();
        for (int a = 0; a < n_actions_; ++a) {
            double q = stage_cost(state, actions_[a], current_price);
            if (q < best_q) {
                best_q = q;
                best_idx = a;
            }
        }
        return actions_[best_idx];
    }

private:
    std::string db_path_;
    sqlite3* db_;
    PumpConfig pump_cfg_;
    PriceJumpDiffusionParams price_params_;
    int n_actions_;
    double gamma_;
    std::vector<double> actions_;
    std::vector<double> value_fn_;

    void open_db() {
        int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Cannot open DB: " + msg);
        }
    }

    void exec_sql(const std::string& sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            throw std::runtime_error("SQLite error: " + msg);
        }
    }

    void install_schema() {
        const char* sql_price =
            "CREATE TABLE IF NOT EXISTS electricity_price_forecast ("
            "  ts TEXT PRIMARY KEY,"
            "  price_per_kwh REAL NOT NULL"
            ");";
        exec_sql(sql_price);

        const char* sql_value =
            "CREATE TABLE IF NOT EXISTS vfd_value_function ("
            "  idx INTEGER PRIMARY KEY,"
            "  reservoir_level REAL NOT NULL,"
            "  value REAL NOT NULL,"
            "  updated_at TEXT NOT NULL"
            ");";
        exec_sql(sql_value);
    }

    void build_action_set() {
        actions_.resize(n_actions_);
        double step = (pump_cfg_.speed_max - pump_cfg_.speed_min) /
                      static_cast<double>(pump_cfg_.speed_levels - 1);
        for (int i = 0; i < n_actions_; ++i) {
            actions_[i] = pump_cfg_.speed_min + step * static_cast<double>(i);
        }
    }

    void init_value_function() {
        std::size_t n_states = 21;
        value_fn_.assign(n_states, 0.0);
    }

    VFDState index_to_state(std::size_t idx) const {
        double level = static_cast<double>(idx) / 20.0;
        VFDState s{level};
        return s;
    }

    double expected_price_next(double price) const {
        double dt = pump_cfg_.dt_hours;
        double drift = price_params_.mu * price * dt;
        double jump_effect = price_params_.lambda * price_params_.jump_mean * dt;
        return price + drift + jump_effect;
    }

    double stage_cost(const VFDState& state, double speed, double price) const {
        double power_kw = speed;
        double energy_kwh = power_kw * pump_cfg_.dt_hours;
        double cost = energy_kwh * price;
        double level_penalty = std::pow(state.reservoir_level - 0.5, 2.0);
        return cost + level_penalty;
    }

    VFDState next_state(const VFDState& state, double speed) const {
        double level = state.reservoir_level + 0.01 * speed * pump_cfg_.dt_hours;
        if (level < 0.0) level = 0.0;
        if (level > 1.0) level = 1.0;
        VFDState s{level};
        return s;
    }

    void policy_evaluation() {
        std::size_t n_states = value_fn_.size();
        std::vector<double> new_values(n_states, 0.0);

        double current_price = load_latest_price();

        for (std::size_t i = 0; i < n_states; ++i) {
            VFDState s = index_to_state(i);
            double v = 0.0;
            for (int a = 0; a < n_actions_; ++a) {
                double cost = stage_cost(s, actions_[a], current_price);
                VFDState s_next = next_state(s, actions_[a]);
                std::size_t j = static_cast<std::size_t>(std::round(s_next.reservoir_level * 20.0));
                if (j >= n_states) j = n_states - 1;
                double v_next = value_fn_[j];
                double q = cost + gamma_ * v_next;
                if (a == 0 || q < v) {
                    v = q;
                }
            }
            new_values[i] = v;
        }

        value_fn_ = new_values;
    }

    void policy_improvement() {
        std::size_t n_states = value_fn_.size();
        double current_price = load_latest_price();

        for (std::size_t i = 0; i < n_states; ++i) {
            VFDState s = index_to_state(i);
            double best_q = std::numeric_limits<double>::infinity();
            double best_speed = actions_[0];
            for (int a = 0; a < n_actions_; ++a) {
                double cost = stage_cost(s, actions_[a], current_price);
                VFDState s_next = next_state(s, actions_[a]);
                std::size_t j = static_cast<std::size_t>(std::round(s_next.reservoir_level * 20.0));
                if (j >= n_states) j = n_states - 1;
                double v_next = value_fn_[j];
                double q = cost + gamma_ * v_next;
                if (q < best_q) {
                    best_q = q;
                    best_speed = actions_[a];
                }
            }
            actions_[best_action_index_for_state(i)] = best_speed;
        }
    }

    std::size_t best_action_index_for_state(std::size_t idx) const {
        return 0;
    }

    double load_latest_price() const {
        const char* sql =
            "SELECT price_per_kwh "
            "FROM electricity_price_forecast "
            "ORDER BY ts DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        double price = 0.1;
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                price = sqlite3_column_double(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
        return price;
    }

    void store_value_function() {
        exec_sql("DELETE FROM vfd_value_function;");
        const char* sql =
            "INSERT INTO vfd_value_function(idx, reservoir_level, value, updated_at) "
            "VALUES(?, ?, ?, datetime('now'));";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return;
        }

        for (std::size_t i = 0; i < value_fn_.size(); ++i) {
            double level = static_cast<double>(i) / 20.0;
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_bind_int(stmt, 1, static_cast<int>(i));
            sqlite3_bind_double(stmt, 2, level);
            sqlite3_bind_double(stmt, 3, value_fn_[i]);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "vfd_jump_diffusion.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    PumpConfig pump_cfg;
    pump_cfg.speed_min = 1.0;
    pump_cfg.speed_max = 10.0;
    pump_cfg.speed_levels = 5;
    pump_cfg.dt_hours = 0.25;

    PriceJumpDiffusionParams price_params;
    price_params.mu = 0.0;
    price_params.sigma2 = 0.01;
    price_params.lambda = 0.1;
    price_params.jump_mean = 0.05;
    price_params.jump_std = 0.02;

    try {
        VFDJumpDiffusionController controller(db_path, pump_cfg, price_params);
        controller.policy_iteration(10);

        VFDState state;
        state.reservoir_level = 0.5;

        double current_price = 0.15;
        double speed = controller.select_pump_speed(state, current_price);
        std::cout << "Selected pump speed=" << speed << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "VFD jump-diffusion controller error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
